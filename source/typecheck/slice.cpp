// slice.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "errors.h"
#include "typecheck.h"

#include "ir/type.h"

using TCS = sst::TypecheckState;


bool sst::getMutabilityOfSliceOfType(fir::Type* ty)
{
	if(ty->isStringType() || ty->isDynamicArrayType())
		return true;

	else if(ty->isArrayType())
		return false;

	else if(ty->isArraySliceType())
		return ty->toArraySliceType()->isMutable();

	else if(ty->isPointerType())
		return ty->isMutablePointer();

	else
		error("Type '%s' does not have mutable variants", ty);
}

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

	else if(ty->isPointerType())
		elm = ty->getPointerElementType();

	else
		error(array, "Invalid type '%s' for slice operation", ty);

	auto begin = this->start ? this->start->typecheck(fs, fir::Type::getInt64()) : 0;
	auto end = this->end ? this->end->typecheck(fs, fir::Type::getInt64()) : 0;

	if(begin && !begin->type->isIntegerType())
		error(begin, "Expected integer type for start index of slice; found '%s'", begin->type);

	if(end && !end->type->isIntegerType())
		error(end, "Expected integer type for end index of slice; found '%s'", end->type);

	//* how it goes:
	// 1. strings and dynamic arrays are always sliced mutably.
	// 2. slices of slices naturally inherit their mutability.
	// 3. arrays are sliced immutably.
	// 4. pointers inherit their mutability as well.

	bool ismut = sst::getMutabilityOfSliceOfType(ty);

	auto ret = new sst::SliceOp(this->loc, fir::ArraySliceType::get(elm, ismut));
	ret->expr = array;
	ret->begin = begin;
	ret->end = end;

	return ret;
}










