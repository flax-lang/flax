// loops.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "sst.h"
#include "codegen.h"

#define dcast(t, v)		dynamic_cast<t*>(v)

CGResult sst::WhileLoop::_codegen(cgn::CodegenState* cs, fir::Type* inferred)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	auto loop = cs->irb.addNewBlockAfter("loop", cs->irb.getCurrentBlock());
	auto merge = cs->irb.addNewBlockAfter("mergerrrrr", cs->irb.getCurrentBlock());


	auto getcond = [](cgn::CodegenState* cs, Expr* c) -> fir::Value* {

		auto cv = cs->oneWayAutocast(c->codegen(cs, fir::Type::getBool()), fir::Type::getBool());
		if(cv.value->getType() != fir::Type::getBool())
			error(c, "Non-boolean expression with type '%s' cannot be used as a conditional", cv.value->getType());

		// ok
		return cs->irb.ICmpEQ(cv.value, fir::ConstantBool::get(true));
	};

	if(this->isDoVariant)
	{
		cs->irb.UnCondBranch(loop);
		cs->irb.setCurrentBlock(loop);

		cs->enterBreakableBody(cgn::ControlFlowPoint(this->body, merge, loop));
		{
			this->body->codegen(cs);
		}
		cs->leaveBreakableBody();

		// ok, check if we have a condition
		if(this->cond)
		{
			iceAssert(this->cond);
			auto condv = getcond(cs, this->cond);
			cs->irb.CondBranch(condv, loop, merge);
		}
		else
		{
			cs->irb.UnCondBranch(merge);
		}
	}
	else
	{
		auto check = cs->irb.addNewBlockAfter("check", cs->irb.getCurrentBlock());
		cs->irb.UnCondBranch(check);
		cs->irb.setCurrentBlock(check);

		// ok
		iceAssert(this->cond);
		auto condv = getcond(cs, this->cond);
		cs->irb.CondBranch(condv, loop, merge);

		cs->irb.setCurrentBlock(loop);

		cs->enterBreakableBody(cgn::ControlFlowPoint(this->body, merge, check));
		{
			this->body->codegen(cs);
		}
		cs->leaveBreakableBody();

		// ok, do a jump back to the top
		cs->irb.UnCondBranch(check);
	}

	cs->irb.setCurrentBlock(merge);

	return CGResult(0);
}


CGResult sst::ForeachLoop::_codegen(cgn::CodegenState* cs, fir::Type* inferred)
{
	return CGResult(0);
}
