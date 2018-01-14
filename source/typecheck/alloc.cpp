// alloc.cpp
// Copyright (c) 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "errors.h"
#include "typecheck.h"

#include "ir/type.h"

sst::Expr* ast::AllocOp::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	fir::Type* elm = fs->convertParserTypeToFIR(this->allocTy);
	iceAssert(elm);

	if(this->isRaw && this->counts.size() > 1)
		error(this, "Only one length dimension is supported for raw memory allocation (have %d)", this->counts.size());

	std::vector<sst::Expr*> counts = util::map(this->counts, [fs](ast::Expr* e) -> auto {
		auto c = e->typecheck(fs, fir::Type::getInt64());
		if(!c->type->isIntegerType())
			error(c, "Expected integer type ('i64') for alloc count, found '%s' instead", c->type);

		return c;
	});

	fir::Type* resType = (this->isRaw || counts.empty() ?
		elm->getPointerTo() : fir::DynamicArrayType::get(elm));

	auto ret = new sst::AllocOp(this->loc, resType);
	ret->elmType = elm;
	ret->counts = counts;
	ret->isRaw = this->isRaw;

	return ret;
}

sst::Stmt* ast::DeallocOp::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	fs->pushLoc(this);
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







