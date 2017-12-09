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

	auto end = cs->oneWayAutocast(this->end->codegen(cs, fir::Type::getInt64()), fir::Type::getInt64()).value;
	iceAssert(end);

	auto step = this->step ?
		cs->oneWayAutocast(this->step->codegen(cs, fir::Type::getInt64()), fir::Type::getInt64()).value :
		fir::ConstantInt::getInt64(1);

	iceAssert(step);

	auto ret = cs->irb.CreateValue(fir::RangeType::get());
	ret = cs->irb.SetRangeLower(ret, start);
	ret = cs->irb.SetRangeUpper(ret, end);
	ret = cs->irb.SetRangeStep(ret, step);

	return CGResult(ret);
}