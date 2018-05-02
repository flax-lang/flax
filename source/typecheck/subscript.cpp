// subscript.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "errors.h"
#include "typecheck.h"

#include "ir/type.h"

TCResult ast::SubscriptOp::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	auto ls = this->expr->typecheck(fs).expr();
	auto rs = this->inside->typecheck(fs).expr();

	// check what's the type
	auto lt = ls->type;
	auto rt = rs->type;

	// make sure the inside is legit
	if(rt->isConstantNumberType())
	{
		if(!mpfr::isint(rt->toConstantNumberType()->getValue()))
			error(this->inside, "Floating-point literal '%s' cannot be used as an subscript index, expected integer type",
				rt->toConstantNumberType()->getValue().toString());

		// ok...
	}
	else if(!rt->isIntegerType())
	{
		error(this->inside, "Subscript index must be an integer type, found '%s'", rt);
	}

	fir::Type* res = 0;

	// check what it is, then
	if(lt->isDynamicArrayType())	res = lt->toDynamicArrayType()->getElementType();
	else if(lt->isArraySliceType())	res = lt->toArraySliceType()->getElementType();
	else if(lt->isPointerType())	res = lt->getPointerElementType();
	else if(lt->isArrayType())		res = lt->toArrayType()->getElementType();
	else if(lt->isStringType())		res = fir::Type::getInt8();
	else							error(this->expr, "Cannot subscript type '%s'", lt);

	iceAssert(res);
	auto ret = new sst::SubscriptOp(this->loc, res);
	ret->expr = ls;
	ret->inside = rs;

	return TCResult(ret);
}











