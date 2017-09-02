// assign.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "errors.h"
#include "typecheck.h"

#include "ir/type.h"

using TCS = sst::TypecheckState;

#define dcast(t, v)		dynamic_cast<t*>(v)

sst::Stmt* ast::AssignOp::typecheck(TCS* fs, fir::Type* infer)
{
	// check the left side
	auto l = dcast(sst::Expr, this->left->typecheck(fs));
	if(!l) error(this->left, "Statement cannot be assigned to; expected expression");

	auto r = dcast(sst::Expr, this->right->typecheck(fs, l->type));
	if(!r)							error(this->right, "Statement cannot be used as an expression");
	else if(r->type->isVoidType())	error(this->right, "Value has void type");

	// check if we can do it first
	auto lt = l->type;
	auto rt = r->type;

	if(this->op != Operator::Assign)
	{
		auto nonass = getNonAssignOp(this->op);
		if(fs->getBinaryOpResultType(lt, rt, nonass) == 0)
		{
			error(this, "Unsupported operator '%s' between types '%s' and '%s', in compound assignment operator '%s'",
				operatorToString(nonass), lt->str(), rt->str(), operatorToString(this->op));
		}
	}

	if(lt != rt)
	{
		HighlightOptions hs;
		hs.underlines.push_back(this->left->loc);
		hs.underlines.push_back(this->right->loc);

		error(this, hs, "Cannot assign value of type '%s' to expected type '%s'", rt->str(), lt->str());
	}

	auto ret = new sst::AssignOp(this->loc);
	ret->op = this->op;
	ret->left = l;
	ret->right = r;

	ret->type = fir::Type::getVoid();
	return ret;
}
