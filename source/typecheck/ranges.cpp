// ranges.cpp
// Copyright (c) 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "errors.h"
#include "typecheck.h"

#include "ir/type.h"

#include "memorypool.h"

TCResult ast::RangeExpr::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	auto ret = util::pool<sst::RangeExpr>(this->loc, fir::RangeType::get());
	ret->halfOpen = this->halfOpen;

	ret->start = this->start->typecheck(fs, fir::Type::getNativeWord()).expr();
	if(!ret->start->type->isIntegerType())
		error(ret->start, "expected integer type in range expression (start), found '%s' instead", ret->start->type);

	ret->end = this->end->typecheck(fs, fir::Type::getNativeWord()).expr();
	if(!ret->end->type->isIntegerType())
		error(ret->end, "expected integer type in range expression (end), found '%s' instead", ret->end->type);

	if(this->step)
	{
		ret->step = this->step->typecheck(fs, fir::Type::getNativeWord()).expr();
		if(!ret->step->type->isIntegerType())
			error(ret->step, "expected integer type in range expression (step), found '%s' instead", ret->step->type);
	}

	return TCResult(ret);
}
