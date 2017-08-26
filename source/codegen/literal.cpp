// literal.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "sst.h"
#include "codegen.h"


CGResult sst::LiteralDec::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	error(this, "not implemented");
}

CGResult sst::LiteralInt::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	// todo: do some proper thing
	if(this->negative)
		return CGResult(fir::ConstantInt::getInt64(-1 * (int64_t) this->number));

	else if(this->number > INT64_MAX)
		return CGResult(fir::ConstantInt::getUint64(this->number));

	else
		return CGResult(fir::ConstantInt::getInt64(this->number));
}

CGResult sst::LiteralNull::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	return CGResult(fir::ConstantValue::getNull());
}

CGResult sst::LiteralBool::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	return CGResult(fir::ConstantInt::getBool(this->value));
}

CGResult sst::LiteralTuple::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	error(this, "not implemented");
}

CGResult sst::LiteralString::_codegen(cgn::CodegenState* cs, fir::Type* infer)
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

	}

	error(this, "not implemented");
}















