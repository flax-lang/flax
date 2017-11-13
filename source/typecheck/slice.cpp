// slice.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "errors.h"
#include "typecheck.h"

#include "ir/type.h"

using TCS = sst::TypecheckState;
#define dcast(t, v)		dynamic_cast<t*>(v)

sst::Expr* ast::SliceOp::typecheck(sst::TypecheckState* fs, fir::Type* inferred)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	auto array = this->expr->typecheck(fs);
	auto ty = array->type;

	fir::Type* elm = 0;
	if(ty->isDynamicArrayType() || ty->isArraySliceType() || ty->isArrayType())
		elm = ty->getArrayElementType();

	else if(ty->isStringType())
		elm = fir::Type::getChar();

	else
		error(array, "Invalid type '%s' for slice operation", ty->str());

	auto begin = this->start ? this->start->typecheck(fs, fir::Type::getInt64()) : 0;
	auto end = this->end ? this->end->typecheck(fs, fir::Type::getInt64()) : 0;

	if(begin && !begin->type->isIntegerType())
		error(begin, "Expected integer type for start index of slice; found '%s'", begin->type->str());

	if(end && !end->type->isIntegerType())
		error(end, "Expected integer type for end index of slice; found '%s'", end->type->str());

	auto ret = new sst::SliceOp(this->loc, fir::ArraySliceType::get(elm));
	ret->expr = array;
	ret->begin = begin;
	ret->end = end;

	return ret;
}















