// Dependencies.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "../include/ast.h"
#include "../include/codegen.h"
#include "../include/compiler.h"
#include "../include/dependency.h"

using namespace Codegen;
using namespace Ast;

namespace Codegen
{
	static void stronglyConnect(DependencyGraph* graph, int& index, std::deque<std::deque<DepNode*>>& connected, DepNode* node);

	std::deque<std::deque<DepNode*>> DependencyGraph::findCyclicDependencies()
	{
		int index = 0;
		std::deque<std::deque<DepNode*>> ret;

		for(auto n : this->nodes)
		{
			n->index = -1;
			n->onStack = false;
			n->lowlink = -1;
		}

		for(auto n : this->nodes)
		{
			if(n->index == -1)
				stronglyConnect(this, index, ret, n);
		}

		return ret;
	}


	static void stronglyConnect(DependencyGraph* graph, int& index, std::deque<std::deque<DepNode*>>& connected, DepNode* node)
	{
		node->index = index;
		node->lowlink = index;

		index++;


		graph->stack.push(node);
		node->onStack = true;


		std::deque<Dep*> edges = graph->edgesFrom[node];
		for(auto edge : edges)
		{
			DepNode* w = edge->to;
			if(w->index == -1)
			{
				stronglyConnect(graph, index, connected, w);
				node->lowlink = (node->lowlink < w->lowlink ? node->lowlink : w->lowlink);
			}
			else if(w->onStack)
			{
				node->lowlink = (node->lowlink < w->index ? node->lowlink : w->index);
			}
		}



		if(node->lowlink == node->index)
		{
			std::deque<DepNode*> set;

			while(true)
			{
				DepNode* w = graph->stack.top();
				graph->stack.pop();

				w->onStack = false;

				set.push_back(w);

				if(w == node)
					break;
			}

			connected.push_back(set);
		}
	}
}

namespace SemAnalysis
{
	static Dep* createDependencies(std::deque<DepNode*>& nodes, std::map<DepNode*, std::deque<Dep*>>& edges, Expr* from, Expr* expr);

	DependencyGraph* resolveDependencyGraph(CodegenInstance* cgi, Root* root)
	{
		DependencyGraph* graph = new DependencyGraph();

		for(auto top : root->topLevelExpressions)
			createDependencies(graph->nodes, graph->edgesFrom, root, top);

		std::deque<std::deque<DepNode*>> groups = graph->findCyclicDependencies();
		for(std::deque<DepNode*> group : groups)
		{
			if(group.size() > 1)
				error(cgi, group.front()->expr, "cycle");
		}


		return graph;
	}





	static void _createDep(std::deque<DepNode*>& nodes, std::map<DepNode*, std::deque<Dep*>>& edgesFrom, Dep* d, Expr* src, Expr* ex, std::string dest)
	{
		DepNode* from = new DepNode();
		DepNode* to = new DepNode();

		from->expr = src;
		to->name = dest;
		to->users.push_back({ from, src });

		d->from = from;
		d->to = to;

		nodes.push_back(from);
		nodes.push_back(to);

		edgesFrom[from].push_back(d);
	}

	static void createIdentifierDep(std::deque<DepNode*>& nodes, std::map<DepNode*, std::deque<Dep*>>& edges, Dep* d, Expr* src,
		Expr* ex, std::string name)
	{
		_createDep(nodes, edges, d, src, ex, name);
		d->type = DepType::Identifier;
	}

	static void createTypeDep(std::deque<DepNode*>& nodes, std::map<DepNode*, std::deque<Dep*>>& edges, Dep* d, Expr* src, std::string name)
	{
		_createDep(nodes, edges, d, src, 0, name);
		d->type = DepType::Type;
	}



	static Dep* createDependencies(std::deque<DepNode*>& nodes, std::map<DepNode*, std::deque<Dep*>>& edges, Expr* from, Expr* expr)
	{
		Dep* dep = new Dep();

		if(VarRef* vr = dynamic_cast<VarRef*>(expr))
		{
			createIdentifierDep(nodes, edges, dep, from, vr, vr->name);
		}
		else if(VarDecl* vd = dynamic_cast<VarDecl*>(expr))
		{
			if(vd->type.strType != "Inferred")
			{
				createTypeDep(nodes, edges, dep, vd, vd->type.strType);
			}

			if(vd->initVal)
			{
				createDependencies(nodes, edges, vd, vd->initVal);
			}
		}
		else if(FuncCall* fc = dynamic_cast<FuncCall*>(expr))
		{
			createIdentifierDep(nodes, edges, dep, from, fc, fc->name);

			for(auto p : fc->params)
				createDependencies(nodes, edges, fc, p);
		}
		else if(Func* fn = dynamic_cast<Func*>(expr))
		{
			createDependencies(nodes, edges, fn, fn->decl);

			for(auto ex : fn->block->statements)
				createDependencies(nodes, edges, fn, ex);

			for(auto ex : fn->block->deferredStatements)
				createDependencies(nodes, edges, fn, ex);
		}
		else if(FuncDecl* decl = dynamic_cast<FuncDecl*>(expr))
		{
			if(decl->type.strType != "Void")
				createTypeDep(nodes, edges, dep, decl, decl->type.strType);

			for(auto p : decl->params)
				createDependencies(nodes, edges, decl, p);
		}
		else if(ForeignFuncDecl* ffi = dynamic_cast<ForeignFuncDecl*>(expr))
		{
			createDependencies(nodes, edges, ffi, ffi->decl);
		}
		else if(MemberAccess* ma = dynamic_cast<MemberAccess*>(expr))
		{
			createDependencies(nodes, edges, ma, ma->left);
			createDependencies(nodes, edges, ma, ma->right);
		}
		else if(UnaryOp* uo = dynamic_cast<UnaryOp*>(expr))
		{
			createDependencies(nodes, edges, uo, uo->expr);
		}
		else if(BinOp* bo = dynamic_cast<BinOp*>(expr))
		{
			createDependencies(nodes, edges, bo, bo->left);
			createDependencies(nodes, edges, bo, bo->right);

			// todo: custom operators
		}
		else if(Alloc* al = dynamic_cast<Alloc*>(expr))
		{
			createDependencies(nodes, edges, al, al->count);

			for(auto p : al->params)
				createDependencies(nodes, edges, al, p);

			createTypeDep(nodes, edges, dep, al, al->type.strType);
		}
		else if(Dealloc* da = dynamic_cast<Dealloc*>(expr))
		{
			createDependencies(nodes, edges, da, da->expr);
		}
		else if(Return* ret = dynamic_cast<Return*>(expr))
		{
			createDependencies(nodes, edges, ret, ret->val);
		}
		else if(IfStmt* ifst = dynamic_cast<IfStmt*>(expr))
		{
			for(auto e : ifst->cases)
			{
				createDependencies(nodes, edges, ifst, e.first);
				createDependencies(nodes, edges, ifst, e.second);
			}

			if(ifst->final)
				createDependencies(nodes, edges, ifst, ifst->final);
		}
		else if(Tuple* tup = dynamic_cast<Tuple*>(expr))
		{
			for(auto e : tup->values)
				createDependencies(nodes, edges, tup, e);
		}
		else if(ArrayIndex* ai = dynamic_cast<ArrayIndex*>(expr))
		{
			createDependencies(nodes, edges, ai, ai->arr);
			createDependencies(nodes, edges, ai, ai->index);
		}
		else if(ArrayLiteral* al = dynamic_cast<ArrayLiteral*>(expr))
		{
			for(auto v : al->values)
				createDependencies(nodes, edges, al, v);
		}
		else if(PostfixUnaryOp* puo = dynamic_cast<PostfixUnaryOp*>(expr))
		{
			createDependencies(nodes, edges, puo, puo->expr);

			for(auto v : puo->args)
				createDependencies(nodes, edges, puo, v);
		}
		else if(Struct* str = dynamic_cast<Struct*>(expr))
		{
			for(auto m : str->members)
				createDependencies(nodes, edges, str, m);

			for(auto oo : str->opOverloads)
				createDependencies(nodes, edges, str, oo->func);
		}
		else if(Class* cls = dynamic_cast<Class*>(expr))
		{
			for(auto m : cls->members)
				createDependencies(nodes, edges, cls, m);

			for(auto f : cls->funcs)
				createDependencies(nodes, edges, cls, f);

			// todo: computed properties
		}
		else if(Enumeration* enr = dynamic_cast<Enumeration*>(expr))
		{
			for(auto c : enr->cases)
				createDependencies(nodes, edges, enr, c.second);
		}
		else if(BracedBlock* bb = dynamic_cast<BracedBlock*>(expr))
		{
			for(auto ex : bb->statements)
				createDependencies(nodes, edges, bb, ex);

			for(auto ex : bb->deferredStatements)
				createDependencies(nodes, edges, bb, ex);
		}
		else if(WhileLoop* wl = dynamic_cast<WhileLoop*>(expr))
		{
			createDependencies(nodes, edges, wl, wl->cond);
			createDependencies(nodes, edges, wl, wl->body);
		}
		else if(Typeof* to = dynamic_cast<Typeof*>(expr))
		{
			createDependencies(nodes, edges, to, to->inside);
		}
		else if(NamespaceDecl* nd = dynamic_cast<NamespaceDecl*>(expr))
		{
			createDependencies(nodes, edges, nd, nd->innards);
		}
		else if(dynamic_cast<Import*>(expr) || dynamic_cast<Number*>(expr)
			|| dynamic_cast<StringLiteral*>(expr) || dynamic_cast<DummyExpr*>(expr))
		{
		}
		else
		{
			printf("unknown expr: %s\n", typeid(*expr).name());
		}

		return dep;
	}
}







































