// literal.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "sst.h"
#include "codegen.h"

CGResult sst::LiteralDec::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	// todo: do some proper thing
	if(this->type->isConstantNumberType() && infer)
	{
		if(infer->isConstantNumberType())
			error("stop playing games");

		if(!infer->isFloatingPointType())
			error(this, "Non floating-point type ('%s') inferred for floating-point literal", infer->str());

		else if(!fir::checkFloatingPointLiteralFitsIntoType(infer->toPrimitiveType(), this->number))
			error(this, "Floating-point literal cannot fit into inferred type '%s'", infer->str());

		// ok
		return CGResult(fir::ConstantFP::get(infer, this->number));
	}
	else
	{
		return CGResult(fir::ConstantFP::get(this->type, this->number));
	}
}

CGResult sst::LiteralInt::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	// todo: do some proper thing
	if(this->type->isConstantNumberType() && infer)
	{
		if(infer->isConstantNumberType())
			error("stop playing games");

		if(!infer->isIntegerType())
			error(this, "Non integer type ('%s') inferred for integer literal", infer->str());

		bool fits = false;
		if(this->type->isSignedIntType())
			fits = fir::checkSignedIntLiteralFitsIntoType(infer->toPrimitiveType(), (ssize_t) this->number);

		else
			fits = fir::checkUnsignedIntLiteralFitsIntoType(infer->toPrimitiveType(), this->number);

		if(!fits)
			error(this, "Integer literal cannot fit into inferred type '%s'", infer->str());

		// ok
		return CGResult(fir::ConstantInt::get(infer, this->number));
	}
	else
	{
		return CGResult(fir::ConstantInt::get(this->type, this->number));
	}
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

	// allow automatic coercion of string literals into i8*
	if(this->isCString || (infer && infer == fir::Type::getInt8Ptr()))
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















