// call.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "errors.h"
#include "typecheck.h"

#include "ir/type.h"

using TCS = sst::TypecheckState;

#define dcast(t, v)		dynamic_cast<t*>(v)


namespace sst
{
	static FunctionDecl* resolveFunctionCall(TCS* fs, ast::FunctionCall* fc)
	{
		std::vector<FunctionDecl*> candidates;

		// just check up the thingy
		auto tree = fs->stree;

		while(tree)
		{
			// foreign functions cannot be overloaded, so just get the one if it exists.
			if(auto it = tree->foreignFunctions.find(fc->name); it != tree->foreignFunctions.end())
				candidates.push_back(it->second);

			for(auto func : tree->functions[fc->name])
				candidates.push_back(func);

			tree = tree->parent;
		}


		// for now, fuck me
		if(candidates.size() > 1)
		{
			error(fc, "no overloading yet");
		}
		else if(candidates.empty())
		{
			// defer resolution, maybe it's in another module.
			// return 0;
			error(fc, "No such function named '%s'", fc->name.c_str());
		}

		// yay?
		return candidates[0];
	}
}





sst::Stmt* ast::FunctionCall::typecheck(TCS* fs, fir::Type* inferred)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	auto call = new sst::FunctionCall(this->loc);
	for(auto p : this->args)
	{
		auto st = p->typecheck(fs);
		auto expr = dcast(sst::Expr, st);

		if(!expr)
			error(this->loc, "Statement cannot be used as an expression");

		call->arguments.push_back(expr);
	}

	// resolve the function call here
	call->target = sst::resolveFunctionCall(fs, this);
	call->type = call->target->returnType;
	iceAssert(call->type);

	return call;
}








