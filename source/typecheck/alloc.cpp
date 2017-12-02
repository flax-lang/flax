// alloc.cpp
// Copyright (c) 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "errors.h"
#include "typecheck.h"

#include "ir/type.h"

sst::Expr* ast::AllocOp::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	fs->pushLoc(this->loc);
	defer(fs->popLoc());

	fir::Type* elm = fs->convertParserTypeToFIR(this->allocTy);
	iceAssert(elm);

	sst::Expr* cnt = 0;
	if(this->count)
	{
		cnt = this->count->typecheck(fs, fir::Type::getInt64());
		if(!cnt->type->isIntegerType())
			error(cnt, "Expected integer type ('i64') for alloc count, found '%s' instead", cnt->type);

		// aight.
	}

	fir::Type* resType = (this->isRaw ?
		elm->getPointerTo() : fir::DynamicArrayType::get(elm));

	auto ret = new sst::AllocOp(this->loc, resType);
	ret->elmType = elm;
	ret->count = cnt;
	ret->isRaw = this->isRaw;

	return ret;
}

sst::Stmt* ast::DeallocOp::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	fs->pushLoc(this->loc);
	defer(fs->popLoc());

	auto ex = this->expr->typecheck(fs);
	if(ex->type->isDynamicArrayType())
		error(ex, "Dynamic arrays are reference-counted, and cannot be manually freed");

	else if(!ex->type->isPointerType())
		error(ex, "Expected pointer or dynamic array type to deallocate; found '%s' instead", ex->type);

	auto ret = new sst::DeallocOp(this->loc);
	ret->expr = ex;

	return ret;
}







