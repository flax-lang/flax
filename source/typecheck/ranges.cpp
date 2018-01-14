// ranges.cpp
// Copyright (c) 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "errors.h"
#include "typecheck.h"

#include "ir/type.h"

sst::Expr* ast::RangeExpr::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	auto ret = new sst::RangeExpr(this->loc, fir::RangeType::get());
	ret->halfOpen = this->halfOpen;

	ret->start = this->start->typecheck(fs, fir::Type::getInt64());
	if(!ret->start->type->isIntegerType())
		error(ret->start, "Expected integer type in range expression (start), found '%s' instead", ret->start->type);

	ret->end = this->end->typecheck(fs, fir::Type::getInt64());
	if(!ret->end->type->isIntegerType())
		error(ret->end, "Expected integer type in range expression (end), found '%s' instead", ret->end->type);

	if(this->step)
	{
		ret->step = this->step->typecheck(fs, fir::Type::getInt64());
		if(!ret->step->type->isIntegerType())
			error(ret->step, "Expected integer type in range expression (step), found '%s' instead", ret->step->type);
	}

	return ret;
}