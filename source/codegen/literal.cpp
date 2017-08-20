// literal.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "sst.h"
#include "codegen.h"


CGResult sst::LiteralDec::codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	return CGResult(0);
}

CGResult sst::LiteralInt::codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	return CGResult(0);
}

CGResult sst::LiteralNull::codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	return CGResult(0);
}

CGResult sst::LiteralBool::codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	return CGResult(0);
}

CGResult sst::LiteralTuple::codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	return CGResult(0);
}

CGResult sst::LiteralString::codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	if(this->isCString)
	{
		// good old i8*
		fir::Value* stringVal = cs->module->createGlobalString(this->str);
		return CGResult(stringVal);
	}
	else
	{
		error(this, "not sup");
	}

	return CGResult(0);
}















