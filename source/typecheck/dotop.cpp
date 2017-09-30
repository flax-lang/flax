// dotop.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "errors.h"
#include "typecheck.h"

#include "ir/type.h"

using TCS = sst::TypecheckState;
#define dcast(t, v)		dynamic_cast<t*>(v)

sst::Expr* ast::DotOperator::typecheck(TCS* fs, fir::Type* inferred)
{
	fs->pushLoc(this->loc);
	defer(fs->popLoc());


	auto lhs = this->left->typecheck(fs);
	if(auto ident = dcast(sst::VarRef, lhs))
	{
		auto defs = fs->getDefinitionsWithName(ident->name);
		if(defs.empty())
		{
			error(lhs, "No namespace or type with name '%s'", ident->name);
		}
		else if(defs.size() > 1)
		{
			exitless_error(lhs, "Ambiguous reference to entity with name '%s'", ident->name);
			for(auto d : defs)
				info(d, "Possible reference:");

			doTheExit();
		}

		auto def = defs[0];

		// check the type
		if(auto ns = dcast(sst::NamespaceDefn, def))
		{
			error("yes");
		}
		else
		{
			error("not supported");
		}
	}
	else if(auto scp = dcast(sst::ScopeExpr, lhs))
	{
		error("no");
	}
	else
	{
		error("no");
	}
}










