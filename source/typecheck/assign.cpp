// assign.cpp
// Copyright (c) 2014 - 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "errors.h"
#include "typecheck.h"

#include "ir/type.h"
#include "ir/constant.h"

#include "memorypool.h"

TCResult ast::AssignOp::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	// check the left side
	auto l = this->left->typecheck(fs).expr();
	auto r = this->right->typecheck(fs, l->type).expr();

	if(r->type->isVoidType())
		error(this->right, "value has void type");

	// check if we can do it first
	auto lt = l->type;
	auto rt = r->type;

	bool skipCheck = false;
	if(this->op != Operator::Assign)
	{
		auto nonass = Operator::getNonAssignmentVersion(this->op);
		if(fs->getBinaryOpResultType(lt, rt, nonass) == 0)
		{
			error(this, "unsupported operator '%s' between types '%s' and '%s', in compound assignment operator '%s'",
				nonass, lt, rt, this->op);
		}

		skipCheck = true;
	}


	if(!skipCheck && lt != rt && fir::getCastDistance(rt, lt) < 0)
	{
		SpanError::make(SimpleError::make(this->loc, "cannot assign value of type '%s' to expected type '%s'", rt, lt))
			->add(util::ESpan(this->left->loc, strprintf("type '%s'", lt)))
			->add(util::ESpan(this->right->loc, strprintf("type '%s'", rt)))
			->postAndQuit();
	}

	//* note: check for the special case of assigning to a tuple literal, to allow the (a, b) = (b, a) swapping idiom
	if(auto tuple = dcast(sst::LiteralTuple, l))
	{
		auto ret = util::pool<sst::TupleAssignOp>(this->loc);
		for(auto v : tuple->values)
			ret->lefts.push_back(v);

		ret->right = r;

		return TCResult(ret);
	}
	else
	{
		auto ret = util::pool<sst::AssignOp>(this->loc);
		ret->op = this->op;
		ret->left = l;
		ret->right = r;

		return TCResult(ret);
	}
}






