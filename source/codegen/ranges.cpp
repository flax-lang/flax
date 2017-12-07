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

	return CGResult(0);
}