// subscript.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "errors.h"
#include "typecheck.h"

#include "ir/type.h"

#define dcast(t, v)		dynamic_cast<t*>(v)

sst::Stmt* ast::SubscriptOp::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	auto ls = dcast(sst::Expr, this->expr->typecheck(fs));
	if(!ls) error(this->expr, "Statement cannot be used as an expression");

	// check what's the type
	auto lt = ls->type;

	// can we subscript it?
	// todo: of course, custom things later
	if(!(lt->isDynamicArrayType() || lt->isArraySliceType() || lt->isPointerType()))
		error(this->expr, "Cannot subscript type '%s'", lt->str());


	// check what it is, then
	if(lt->isDynamicArrayType())
	{
		// do a check
	}
	else if(lt->isArraySliceType())
	{
	}
	else if(lt->isPointerType())
	{
	}
	else
	{
		iceAssert(0);
	}
}
