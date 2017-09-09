// subscript.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "errors.h"
#include "typecheck.h"

#include "ir/type.h"

#define dcast(t, v)		dynamic_cast<t*>(v)

sst::Expr* ast::SubscriptOp::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	auto ls = this->expr->typecheck(fs);
	auto rs = this->inside->typecheck(fs);

	// check what's the type
	auto lt = ls->type;
	auto rt = rs->type;

	// can we subscript it?
	// todo: of course, custom things later
	if(!(lt->isDynamicArrayType() || lt->isArraySliceType() || lt->isPointerType() || lt->isArrayType()))
		error(this->expr, "Cannot subscript type '%s'", lt->str());

	// make sure the inside is legit
	if(rt->isConstantNumberType())
	{
		if(!mpfr::isint(rt->toConstantNumberType()->getValue()))
			error(this->inside, "Floating-point literal '%s' cannot be used as an subscript index, expected integer type",
				rt->toConstantNumberType()->getValue().toString());

		// ok...
	}
	else if(!rt->isIntegerType())
		error(this->inside, "Subscript index must be an integer type, found '%s'", rt->str());

	fir::Type* res = 0;

	// check what it is, then
	if(lt->isDynamicArrayType())
		res = lt->toDynamicArrayType()->getElementType();

	else if(lt->isArraySliceType())
		res = lt->toArraySliceType()->getElementType();

	else if(lt->isPointerType())
		res = lt->getPointerElementType();

	else if(lt->isArrayType())
		res = lt->toArrayType()->getElementType();

	else
		iceAssert(0);


	auto ret = new sst::SubscriptOp(this->loc, res);
	ret->expr = ls;
	ret->inside = rs;

	return ret;
}











