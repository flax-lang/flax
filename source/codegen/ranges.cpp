// ranges.cpp
// Copyright (c) 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include "sst.h"
#include "codegen.h"
#include "gluecode.h"

CGResult sst::RangeExpr::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	auto start = cs->oneWayAutocast(this->start->codegen(cs, fir::Type::getNativeWord()).value, fir::Type::getNativeWord());
	iceAssert(start);
	if(!start->getType()->isIntegerType())
		error(this->start, "expected integer type in range expression (start), found '%s' instead", start->getType());

	auto end = cs->oneWayAutocast(this->end->codegen(cs, fir::Type::getNativeWord()).value, fir::Type::getNativeWord());
	iceAssert(end);
	if(!end->getType()->isIntegerType())
		error(this->end, "expected integer type in range expression (end), found '%s' instead", end->getType());

	// if we're half-open, then we need to subtract 1 from the end value.
	// TODO: do we need to check for start > end for half open?
	// it's well documented that we always subtract 1 for half open, but it might be immediately obvious.
	if(this->halfOpen) end = cs->irb.Subtract(end, fir::ConstantInt::getNative(1));


	// if start > end, the automatic step should be -1. else, it should be 1 as normal.
	fir::Value* step = (this->step ?
		cs->oneWayAutocast(this->step->codegen(cs, fir::Type::getNativeWord()).value, fir::Type::getNativeWord()) :
		cs->irb.Select(cs->irb.ICmpLEQ(start, end), fir::ConstantInt::getNative(1), fir::ConstantInt::getNative(-1))
	);

	iceAssert(step);
	if(!step->getType()->isIntegerType())
		error(this->step, "expected integer type in range expression (step), found '%s' instead", step->getType());

	auto ret = cs->irb.CreateValue(fir::RangeType::get());
	ret = cs->irb.SetRangeLower(ret, start);
	ret = cs->irb.SetRangeUpper(ret, end);
	ret = cs->irb.SetRangeStep(ret, step);

	// now that we have all the values, it's time to sanity check these things.
	auto checkf = cgn::glue::misc::getRangeSanityCheckFunction(cs);
	if(checkf) cs->irb.Call(checkf, ret, fir::ConstantCharSlice::get(this->loc.toString()));


	return CGResult(ret);
}















