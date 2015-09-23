// RewriteDotOperators.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "../include/ast.h"
#include "../include/codegen.h"
#include "../include/semantic.h"

#include <map>

using namespace Ast;
using namespace Codegen;


// bad: global state.
// do we really need to care though?

struct GlobalState
{
	CodegenInstance* cgi;
	std::map<MemberAccess*, bool> visitedMAs;

	std::deque<std::string> nsstrs;
	int MAWithinMASearchNesting = 0;
};

static GlobalState gstate;
static void findDotOperator(Expr* expr);


/*

	((a.b).c).d

	RDO[ ((a.b).c).d ]
	{
		RDO[ (a.b).c ]
		{
			RDO[ a.b ]
			{
				if(a is ns)
				{
					a.b = MALeftNS(a.b)
				}
				else
				{
					...
				}
			}


			if(a.b is MALeftNS)
			{
			}
		}
	}

*/



static void rewriteDotOperator(MemberAccess* ma)
{
	CodegenInstance* cgi = gstate.cgi;

	// recurse first -- we need to know what the left side is before anything else.
	if(MemberAccess* left = dynamic_cast<MemberAccess*>(ma->left))
		rewriteDotOperator(left);

	// either we're the leftmost, or all left-sides have been resolved already.
	// what are we?

	if(MemberAccess* leftma = dynamic_cast<MemberAccess*>(ma->left))
	{
		// get the type of the left MA, so we know the type of the left MA's right member.

		// we don't need to know the left side of the ma to know that the right side is a function call
		if(dynamic_cast<FuncCall*>(leftma->right))
		{
			ma->matype = MAType::LeftFunctionCall;
			return;
		}
		else if(VarRef* vr = dynamic_cast<VarRef*>(leftma->right))
		{
			// however, if it's a varref, we need to know... sadly.
			// grab the functree.
			FunctionTree* ft = cgi->getCurrentFuncTree(&gstate.nsstrs);


			// is this a namespace?
			for(auto sub : ft->subs)
			{
				if(sub->nsName == vr->name)
				{
					ma->matype = MAType::LeftNamespace;
					return;
				}
			}

			// type???
			if(cgi->getType(vr->name))
			{
				ma->matype = MAType::LeftTypename;
				return;
			}


			// must be a variable -- idgaf if it's global or not.
			// not relevant to type checking.

			ma->matype = MAType::LeftVariable;
			return;
		}
		else if(dynamic_cast<Number*>(leftma->right))
		{
			// tuple access.
			ma->matype = MAType::LeftVariable;
			return;
		}
		else
		{
			error(cgi, ma, "????");
		}
	}
	else if(dynamic_cast<FuncCall*>(ma->left))
	{
		ma->matype = MAType::LeftFunctionCall;
		return;
	}
	else if(VarRef* vr = dynamic_cast<VarRef*>(ma->left))
	{
		// what kind of vr??
		FunctionTree* ft = cgi->getCurrentFuncTree(&gstate.nsstrs);
		iceAssert(ft);

		for(auto sub : ft->subs)
		{
			if(sub->nsName == vr->name)
			{
				gstate.nsstrs.push_back(vr->name);
				ma->matype = MAType::LeftNamespace;
				return;
			}
		}

		if(cgi->getType(vr->name))
		{
			ma->matype = MAType::LeftTypename;
			return;
		}

		ma->matype = MAType::LeftVariable;
		return;
	}
	else if(dynamic_cast<Tuple*>(ma->left))
	{
		ma->matype = MAType::LeftVariable;
		return;
	}
	else
	{
		error(cgi, ma, "?????");
	}
}





// might be a mess.
static void findDotOperator(Expr* expr)
{
	if(VarDecl* vd = dynamic_cast<VarDecl*>(expr))
	{
		// check for init var.
		if(vd->initVal)
			findDotOperator(vd->initVal);
	}
	else if(BracedBlock* bb = dynamic_cast<BracedBlock*>(expr))
	{
		for(auto e : bb->statements)
			findDotOperator(e);

		for(auto e : bb->deferredStatements)
			findDotOperator(e);
	}
	else if(BinOp* bin = dynamic_cast<BinOp*>(expr))
	{
		findDotOperator(bin->left);
		findDotOperator(bin->right);
	}
	else if(UnaryOp* unr = dynamic_cast<UnaryOp*>(expr))
	{
		findDotOperator(unr->expr);
	}
	else if(Func* fn = dynamic_cast<Func*>(expr))
	{
		findDotOperator(fn->block);
	}
	else if(FuncCall* fncall = dynamic_cast<FuncCall*>(expr))
	{
		// special case -- this can appear in a MA list
		// and we must be able to find MAs within the parameter list

		int old = gstate.MAWithinMASearchNesting;
		gstate.MAWithinMASearchNesting = 0;

		for(auto e : fncall->params)
			findDotOperator(e);

		gstate.MAWithinMASearchNesting = old;
	}
	else if(If* ifexpr = dynamic_cast<If*>(expr))
	{
		for(auto e : ifexpr->cases)
		{
			findDotOperator(e.first);
			findDotOperator(e.second);
		}

		if(ifexpr->final)
			findDotOperator(ifexpr->final);
	}
	else if(WhileLoop* whileloop = dynamic_cast<WhileLoop*>(expr))
	{
		findDotOperator(whileloop->cond);
		findDotOperator(whileloop->body);
	}
	else if(Return* ret = dynamic_cast<Return*>(expr))
	{
		if(ret->val)
			findDotOperator(ret->val);
	}
	else if(ArrayIndex* ari = dynamic_cast<ArrayIndex*>(expr))
	{
		findDotOperator(ari->arr);
		findDotOperator(ari->index);
	}
	else if(StructBase* sb = dynamic_cast<StructBase*>(expr))
	{
		for(auto mem : sb->members)
		{
			if(mem->initVal)
				findDotOperator(mem->initVal);
		}

		for(auto op : sb->opOverloads)
			findDotOperator(op->func);

		for(auto fn : sb->funcs)
			findDotOperator(fn);

		for(auto s : sb->nestedTypes)
			findDotOperator(s.first);

		for(auto c : sb->cprops)
		{
			if(c->getter)
				findDotOperator(c->getter);

			if(c->setter)
				findDotOperator(c->setter);
		}
	}
	else if(MemberAccess* ma = dynamic_cast<MemberAccess*>(expr))
	{
		gstate.MAWithinMASearchNesting++;
		findDotOperator(ma->left);
		findDotOperator(ma->right);
		gstate.MAWithinMASearchNesting--;


		// we never recursively do stuff to this -- we only ever want the topmost level.
		if(gstate.MAWithinMASearchNesting == 0)
		{
			if(gstate.visitedMAs.find(ma) == gstate.visitedMAs.end())
				gstate.visitedMAs[ma] = true;
		}
	}
	else
	{

	}
}

namespace SemAnalysis
{
	void rewriteDotOperators(CodegenInstance* cgi)
	{
		gstate.cgi = cgi;

		Root* rootNode = cgi->rootNode;

		// find all the dot operators (recursive)
		for(Expr* e : rootNode->topLevelExpressions)
			findDotOperator(e);

		for(auto pair : gstate.visitedMAs)
		{
			rewriteDotOperator(pair.first);
			gstate.nsstrs.clear();
		}


		gstate = GlobalState();
	}

}










