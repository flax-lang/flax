// enums.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "errors.h"
#include "typecheck.h"

#include "ir/type.h"

using TCS = sst::TypecheckState;
#define dcast(t, v)		dynamic_cast<t*>(v)

sst::Stmt* ast::EnumDefn::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	auto defn = new sst::EnumDefn(this->loc);
	defn->id = Identifier(this->name, IdKind::Type);
	defn->id.scope = fs->getCurrentScope();


	fir::Type* base = (this->memberType ? fs->convertParserTypeToFIR(this->memberType) : nullptr);
	defn->memberType = base;

	for(auto cs : this->cases)
	{
		sst::Expr* val = 0;
		if(cs.value)
		{
			iceAssert(base);
			val = cs.value->typecheck(fs, base);

			if(val->type != base)
				error(cs.value, "Mismatched type in enum case value; expected type '%s', but found type '%s'", base, val->type);
		}

		defn->cases.push_back(sst::EnumDefn::Case { .name = cs.name, .value = val });
	}

	return defn;
}
