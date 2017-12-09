// ranges.cpp
// Copyright (c) 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "sst.h"
#include "codegen.h"
#include "platform.h"

#define dcast(t, v)		dynamic_cast<t*>(v)

CGResult sst::RangeExpr::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this->loc);
	defer(cs->popLoc());

	auto start = cs->oneWayAutocast(this->start->codegen(cs, fir::Type::getInt64()), fir::Type::getInt64()).value;
	iceAssert(start);
	if(!start->getType()->isIntegerType())
		error(this->start, "Expected integer type in range expression (start), found '%s' instead", start->getType());

	auto end = cs->oneWayAutocast(this->end->codegen(cs, fir::Type::getInt64()), fir::Type::getInt64()).value;
	iceAssert(end);
	if(!end->getType()->isIntegerType())
		error(this->end, "Expected integer type in range expression (end), found '%s' instead", end->getType());

	// if we're half-open, then we need to subtract 1 from the end value.
	// TODO: do we need to check for start > end for half open?
	// it's well documented that we always subtract 1 for half open, but it might be immediately obvious.
	if(this->halfOpen) end = cs->irb.Subtract(end, fir::ConstantInt::getInt64(1));

	auto step = this->step ?
		cs->oneWayAutocast(this->step->codegen(cs, fir::Type::getInt64()), fir::Type::getInt64()).value :
		fir::ConstantInt::getInt64(1);

	iceAssert(step);
	if(!step->getType()->isIntegerType())
		error(this->step, "Expected integer type in range expression (step), found '%s' instead", step->getType());


	auto ret = cs->irb.CreateValue(fir::RangeType::get());
	ret = cs->irb.SetRangeLower(ret, start);
	ret = cs->irb.SetRangeUpper(ret, end);
	ret = cs->irb.SetRangeStep(ret, step);

	return CGResult(ret);
}