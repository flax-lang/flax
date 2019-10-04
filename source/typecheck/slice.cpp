// slice.cpp
// Copyright (c) 2014 - 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "errors.h"
#include "typecheck.h"

#include "ir/type.h"

#include "memorypool.h"

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
		error("type '%s' does not have mutable variants", ty);
}

TCResult ast::SliceOp::typecheck(sst::TypecheckState* fs, fir::Type* inferred)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	fs->pushAnonymousTree();
	defer(fs->popTree());

	auto array = this->expr->typecheck(fs).expr();
	auto ty = array->type;

	fs->enterSubscript(array);
	defer(fs->leaveSubscript());

	fir::Type* elm = 0;
	if(ty->isDynamicArrayType() || ty->isArraySliceType() || ty->isArrayType())
		elm = ty->getArrayElementType();

	else if(ty->isStringType())
		elm = fir::Type::getInt8();

	else if(ty->isPointerType())
		elm = ty->getPointerElementType();

	else
		error(array, "invalid type '%s' for slice operation", ty);

	auto begin = this->start ? this->start->typecheck(fs, fir::Type::getNativeWord()).expr() : 0;
	auto end = this->end ? this->end->typecheck(fs, fir::Type::getNativeWord()).expr() : 0;

	if(begin && !begin->type->isIntegerType())
		error(begin, "expected integer type for start index of slice; found '%s'", begin->type);

	if(end && !end->type->isIntegerType())
		error(end, "expected integer type for end index of slice; found '%s'", end->type);

	//* how it goes:
	// 1. strings and dynamic arrays are always sliced mutably.
	// 2. slices of slices naturally inherit their mutability.
	// 3. arrays are sliced immutably.
	// 4. pointers inherit their mutability as well.

	bool ismut = sst::getMutabilityOfSliceOfType(ty);

	auto ret = util::pool<sst::SliceOp>(this->loc, fir::ArraySliceType::get(elm, ismut));
	ret->expr = array;
	ret->begin = begin;
	ret->end = end;

	return TCResult(ret);
}










