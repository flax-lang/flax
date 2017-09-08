// assign.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "sst.h"
#include "codegen.h"

#define dcast(t, v)		dynamic_cast<t*>(v)

CGResult sst::AssignOp::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	auto lr = this->left->codegen(cs);
	auto lt = lr.value->getType();

	if(!lr.pointer || lr.kind != CGResult::VK::LValue)
	{
		HighlightOptions hs;
		hs.underlines.push_back(this->left->loc);
		error(this, hs, "Cannot assign to non-lvalue");
	}

	if(lr.pointer && lr.pointer->isImmutable())
	{
		HighlightOptions hs;
		hs.underlines.push_back(this->left->loc);
		error(this, hs, "Cannot assign to immutable expression");
	}

	// okay, i guess
	auto rr = this->right->codegen(cs, lt);

	if(this->op != Operator::Assign)
	{
		// ok it's a compound assignment
		auto [ newl, newr ] = cs->autoCastValueTypes(lr, rr);
		Operator nonass = getNonAssignOp(this->op);

		// do the op first
		auto res = cs->performBinaryOperation(this->loc, { this->left->loc, newl }, { this->right->loc, newr }, nonass);

		// assign the res to the thing
		rr = res;
	}

	rr = cs->oneWayAutocast(rr, lt);

	if(rr.value == 0)
	{
		error(this, "Invalid assignment from value of type '%s' to expected type '%s'", rr.value->getType()->str(),
			lt->str());
	}

	// ok then
	if(lt != rr.value->getType())
		error(this, "What? left = %s, right = %s", lt->str(), rr.value->getType()->str());

	iceAssert(lr.pointer);
	iceAssert(rr.value->getType() == lr.pointer->getType()->getPointerElementType());

	// store
	cs->irb.CreateStore(rr.value, lr.pointer);

	return CGResult(0);
}















