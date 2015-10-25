// Dependencies.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "codegen.h"
#include "compiler.h"
#include "dependency.h"

#include <fstream>
#include <sstream>

using namespace Codegen;
using namespace Ast;

namespace Codegen
{
	static void stronglyConnect(DependencyGraph* graph, int& index, std::deque<std::deque<DepNode*>>& connected, DepNode* node);

	void DependencyGraph::addModuleDependency(std::string from, std::string to, Expr* imp)
	{
		// find existing node
		DepNode* src = 0;
		DepNode* dst = 0;
		for(auto d : this->nodes)
		{
			if(!src && d->name == from)
			{
				src = d;
			}
			else if(!dst && d->name == to)
			{
				dst = d;
			}
		}

		if(!src)
		{
			src = new DepNode();
			src->name = from;

			this->nodes.push_back(src);
		}

		if(!dst)
		{
			dst = new DepNode();
			dst->name = to;

			this->nodes.push_back(dst);
		}

		dst->users.push_back({ src, imp });

		Dep* d = new Dep();
		d->type = DepType::Module;
		d->from = src;
		d->to = dst;

		this->edgesFrom[src].push_back(d);
	}



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


	std::deque<DepNode*> DependencyGraph::findDependenciesOf(Expr* expr)
	{
		std::deque<DepNode*> ret;
		for(auto e : this->edgesFrom)
		{
			if(e.first->expr == expr)
			{
				for(auto d : e.second)
					ret.push_back(d->to);
			}
		}

		return ret;
	}
}

namespace SemAnalysis
{
	static Dep* createDependencies(DependencyGraph* graph, Expr* from, Expr* expr);

	DependencyGraph* resolveDependencyGraph(CodegenInstance* cgi, Root* root)
	{
		DependencyGraph* graph = new DependencyGraph();

		for(auto top : root->topLevelExpressions)
			createDependencies(graph, root, top);

		// this is destructive for the graph, so.
		DependencyGraph* tmp = new DependencyGraph();
		tmp->edgesFrom = graph->edgesFrom;
		tmp->nodes = graph->nodes;

		std::deque<std::deque<DepNode*>> groups = tmp->findCyclicDependencies();
		for(std::deque<DepNode*> group : groups)
		{
			if(group.size() > 1)
				error(group.front()->expr, "cycle");
		}

		return graph;
	}





	static void _createDep(DependencyGraph* graph, Dep* d, Expr* src, Expr* ex, std::string dest)
	{
		// find existing node
		DepNode* from = 0;
		for(auto d : graph->nodes)
		{
			if(!from && d->expr == src)
			{
				from = d;
			}
		}

		if(!from)
		{
			from = new DepNode();
			from->expr = src;
			graph->nodes.push_back(from);
		}


		DepNode* to = new DepNode();
		to->name = dest;
		graph->nodes.push_back(to);

		to->users.push_back({ from, src });

		d->from = from;
		d->to = to;

		graph->edgesFrom[from].push_back(d);
	}

	static void createFunctionDep(DependencyGraph* graph, Dep* d, Expr* src, Expr* ex, std::string name)
	{
		_createDep(graph, d, src, ex, name);
		d->type = DepType::Function;
	}

	static void createExprDep(DependencyGraph* graph, Dep* d, Expr* src, Expr* dest)
	{
		// _createDep(graph, d, src, dest, "");
		// d->type = DepType::Value;
	}

	static void createTypeDep(DependencyGraph* graph, Dep* d, Expr* src, std::string name)
	{
		_createDep(graph, d, src, 0, name);
		d->type = DepType::Type;
	}



	static Dep* createDependencies(DependencyGraph* graph, Expr* from, Expr* expr)
	{
		createExprDep(graph, new Dep(), from, expr);

		Dep* dep = new Dep();

		if(VarRef* vr = dynamic_cast<VarRef*>(expr))
		{
			(void) vr;
			// createIdentifierDep(graph, dep, from, vr, vr->name);
		}
		else if(VarDecl* vd = dynamic_cast<VarDecl*>(expr))
		{
			if(vd->type.strType != "Inferred")
			{
				// info(vd, "creating type dep on %s (vardecl)\n", vd->type.strType.c_str());
				createTypeDep(graph, dep, vd, vd->type.strType);
			}

			if(vd->initVal)
			{
				// info(vd, "init val dep\n");
				createDependencies(graph, vd, vd->initVal);
			}
		}
		else if(FuncCall* fc = dynamic_cast<FuncCall*>(expr))
		{
			createFunctionDep(graph, dep, from, fc, fc->name);

			for(auto p : fc->params)
				createDependencies(graph, fc, p);
		}
		else if(Func* fn = dynamic_cast<Func*>(expr))
		{
			createDependencies(graph, fn, fn->decl);

			for(auto ex : fn->block->statements)
				createDependencies(graph, fn, ex);

			for(auto ex : fn->block->deferredStatements)
				createDependencies(graph, fn, ex);
		}
		else if(FuncDecl* decl = dynamic_cast<FuncDecl*>(expr))
		{
			if(decl->type.strType != "Void")
			{
				// info(decl, "creating type dep on %s\n", decl->type.strType.c_str());
				createTypeDep(graph, dep, decl, decl->type.strType);
			}

			for(auto p : decl->params)
				createDependencies(graph, decl, p);
		}
		else if(ForeignFuncDecl* ffi = dynamic_cast<ForeignFuncDecl*>(expr))
		{
			createDependencies(graph, ffi, ffi->decl);
		}
		else if(MemberAccess* ma = dynamic_cast<MemberAccess*>(expr))
		{
			createDependencies(graph, ma, ma->left);
			createDependencies(graph, ma, ma->right);
		}
		else if(UnaryOp* uo = dynamic_cast<UnaryOp*>(expr))
		{
			createDependencies(graph, uo, uo->expr);
		}
		else if(BinOp* bo = dynamic_cast<BinOp*>(expr))
		{
			createDependencies(graph, bo, bo->left);
			createDependencies(graph, bo, bo->right);

			// todo: custom operators
		}
		else if(Alloc* al = dynamic_cast<Alloc*>(expr))
		{
			for(auto c : al->counts)
				createDependencies(graph, al, c);

			for(auto p : al->params)
				createDependencies(graph, al, p);

			createTypeDep(graph, dep, al, al->type.strType);
		}
		else if(Dealloc* da = dynamic_cast<Dealloc*>(expr))
		{
			createDependencies(graph, da, da->expr);
		}
		else if(Return* ret = dynamic_cast<Return*>(expr))
		{
			createDependencies(graph, ret, ret->val);
		}
		else if(IfStmt* ifst = dynamic_cast<IfStmt*>(expr))
		{
			for(auto e : ifst->cases)
			{
				createDependencies(graph, ifst, e.first);
				createDependencies(graph, ifst, e.second);
			}

			if(ifst->final)
				createDependencies(graph, ifst, ifst->final);
		}
		else if(Tuple* tup = dynamic_cast<Tuple*>(expr))
		{
			for(auto e : tup->values)
				createDependencies(graph, tup, e);
		}
		else if(ArrayIndex* ai = dynamic_cast<ArrayIndex*>(expr))
		{
			createDependencies(graph, ai, ai->arr);
			createDependencies(graph, ai, ai->index);
		}
		else if(ArrayLiteral* al = dynamic_cast<ArrayLiteral*>(expr))
		{
			for(auto v : al->values)
				createDependencies(graph, al, v);
		}
		else if(PostfixUnaryOp* puo = dynamic_cast<PostfixUnaryOp*>(expr))
		{
			createDependencies(graph, puo, puo->expr);

			for(auto v : puo->args)
				createDependencies(graph, puo, v);
		}
		else if(Struct* str = dynamic_cast<Struct*>(expr))
		{
			for(auto m : str->members)
				createDependencies(graph, str, m);

			for(auto oo : str->opOverloads)
				createDependencies(graph, str, oo->func);
		}
		else if(Class* cls = dynamic_cast<Class*>(expr))
		{
			for(auto m : cls->members)
				createDependencies(graph, cls, m);

			for(auto f : cls->funcs)
				createDependencies(graph, cls, f);

			// todo: computed properties
		}
		else if(Enumeration* enr = dynamic_cast<Enumeration*>(expr))
		{
			for(auto c : enr->cases)
				createDependencies(graph, enr, c.second);
		}
		else if(BracedBlock* bb = dynamic_cast<BracedBlock*>(expr))
		{
			for(auto ex : bb->statements)
				createDependencies(graph, bb, ex);

			for(auto ex : bb->deferredStatements)
				createDependencies(graph, bb, ex);
		}
		else if(WhileLoop* wl = dynamic_cast<WhileLoop*>(expr))
		{
			createDependencies(graph, wl, wl->cond);
			createDependencies(graph, wl, wl->body);
		}
		else if(Typeof* to = dynamic_cast<Typeof*>(expr))
		{
			createDependencies(graph, to, to->inside);
		}
		else if(NamespaceDecl* nd = dynamic_cast<NamespaceDecl*>(expr))
		{
			createDependencies(graph, nd, nd->innards);
		}
		else if(dynamic_cast<Import*>(expr) || dynamic_cast<Number*>(expr)
			|| dynamic_cast<StringLiteral*>(expr) || dynamic_cast<DummyExpr*>(expr))
		{
		}
		else
		{
			// printf("unknown expr: %s\n", typeid(*expr).name());
		}

		return dep;
	}
}







































