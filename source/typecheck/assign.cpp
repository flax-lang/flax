// assign.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "errors.h"
#include "typecheck.h"

#include "ir/type.h"
#include "ir/constant.h"

using TCS = sst::TypecheckState;

sst::Expr* ast::AssignOp::typecheck(TCS* fs, fir::Type* infer)
{
	// check the left side
	auto l = this->left->typecheck(fs);
	auto r = this->right->typecheck(fs, l->type);

	if(r->type->isVoidType())	error(this->right, "Value has void type");

	// check if we can do it first
	auto lt = l->type;
	auto rt = r->type;

	bool skipCheck = false;
	if(this->op != Operator::Assign)
	{
		auto nonass = getNonAssignOp(this->op);
		if(fs->getBinaryOpResultType(lt, rt, nonass) == 0)
		{
			error(this, "Unsupported operator '%s' between types '%s' and '%s', in compound assignment operator '%s'",
				operatorToString(nonass), lt, rt, operatorToString(this->op));
		}

		skipCheck = true;
	}

	if(rt->isConstantNumberType() && lt->isPrimitiveType())
	{
		auto num = rt->toConstantNumberType()->getValue();
		if(fir::checkLiteralFitsIntoType(lt->toPrimitiveType(), num))
			skipCheck = true;

		else
			warn(this, "nofit");
	}

	if(!skipCheck && lt != rt)
	{
		HighlightOptions hs;
		// hs.drawCaret = false;
		hs.underlines.push_back(this->left->loc);
		hs.underlines.push_back(this->right->loc);

		error(this, hs, "Cannot assign value of type '%s' to expected type '%s'", rt, lt);
	}

	auto ret = new sst::AssignOp(this->loc);
	ret->op = this->op;
	ret->left = l;
	ret->right = r;

	return ret;
}






