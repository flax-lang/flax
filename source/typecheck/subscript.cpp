// subscript.cpp
// Copyright (c) 2014 - 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "errors.h"
#include "typecheck.h"

#include "ir/type.h"
#include "memorypool.h"

TCResult ast::SubscriptOp::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	fs->pushAnonymousTree();
	defer(fs->popTree());

	auto ls = this->expr->typecheck(fs).expr();
	fs->enterSubscript(ls);
	defer(fs->leaveSubscript());

	auto rs = this->inside->typecheck(fs).expr();

	// check what's the type
	auto lt = ls->type;
	auto rt = rs->type;

	if((rt->isConstantNumberType() && rt->toConstantNumberType()->isFloating()) && !rt->isIntegerType())
		error(this->inside, "subscript index must be an integer type, found '%s'", rt);


	fir::Type* res = 0;

	// check what it is, then
	if(lt->isDynamicArrayType())	res = lt->toDynamicArrayType()->getElementType();
	else if(lt->isArraySliceType())	res = lt->toArraySliceType()->getElementType();
	else if(lt->isPointerType())	res = lt->getPointerElementType();
	else if(lt->isArrayType())		res = lt->toArrayType()->getElementType();
	else if(lt->isStringType())		res = fir::Type::getInt8();
	else							error(this->expr, "cannot subscript type '%s'", lt);

	iceAssert(res);
	auto ret = util::pool<sst::SubscriptOp>(this->loc, res);
	ret->expr = ls;
	ret->inside = rs;

	return TCResult(ret);
}


TCResult ast::SubscriptDollarOp::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	if(!fs->isInSubscript())
		error(this, "invalid use of '$' in non-subscript context");

	else if(auto arr = fs->getCurrentSubscriptArray();
		arr->type->isPointerType() || !(arr->type->isArraySliceType() || arr->type->isArrayType() || arr->type->isStringType()
			|| arr->type->isDynamicArrayType()))
	{
		SpanError::make(SimpleError::make(this->loc, "invalid use of '$' on subscriptee with %stype '%s'",
			arr->type->isPointerType() ? "pointer " : "", arr->type))
			->add(util::ESpan(arr->loc, "here"))->postAndQuit();
	}

	return TCResult(util::pool<sst::SubscriptDollarOp>(this->loc, fir::Type::getNativeWord()));
}








