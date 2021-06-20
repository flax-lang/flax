// sizeof.cpp
// Copyright (c) 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "pts.h"
#include "errors.h"
#include "ir/type.h"
#include "typecheck.h"

#include "memorypool.h"

TCResult ast::SizeofOp::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	auto ret = util::pool<sst::SizeofOp>(this->loc, fir::Type::getNativeWord());

	if(dcast(ast::LitNumber, this->expr))
	{
		error(this->expr, "literal numbers cannot be sized");
	}

	this->expr->checkAsType = true;
	fir::Type* out = this->expr->typecheck(fs).expr()->type;

	iceAssert(out);
	ret->typeToSize = out;

	return TCResult(ret);
}


TCResult ast::TypeidOp::typecheck(sst::TypecheckState* fs, fir::Type* inferred)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	auto ret = util::pool<sst::TypeidOp>(this->loc, fir::Type::getNativeUWord());

	if(dcast(ast::LitNumber, this->expr))
	{
		error(this->expr, "literal numbers cannot be typeid'd");
	}

	this->expr->checkAsType = true;
	fir::Type* out = this->expr->typecheck(fs).expr()->type;

	iceAssert(out);
	ret->typeToId = out;

	return TCResult(ret);
}



