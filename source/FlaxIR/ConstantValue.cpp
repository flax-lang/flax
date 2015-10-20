// ConstantValue.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ir/value.h"
#include "ir/constant.h"

namespace fir
{
	ConstantValue::ConstantValue(Type* t) : Value(t)
	{
		// nothing.
	}

	ConstantValue* ConstantValue::getNullValue(Type* type)
	{
		return new ConstantValue(type);
	}


	// todo: unique these values.
	ConstantInt* ConstantInt::getSigned(Type* intType, ssize_t val)
	{
		iceAssert(intType->isIntegerType() && "not integer type");
		return new ConstantInt(intType, val);
	}

	ConstantInt* ConstantInt::getUnsigned(Type* intType, size_t val)
	{
		iceAssert(intType->isIntegerType() && "not integer type");
		return new ConstantInt(intType, val);
	}

	ConstantInt::ConstantInt(Type* type, ssize_t val) : ConstantValue(type)
	{
		this->value = val;
	}

	ConstantInt::ConstantInt(Type* type, size_t val) : ConstantValue(type)
	{
		this->value = val;
	}

	ssize_t ConstantInt::getSignedValue()
	{
		return (ssize_t) this->value;
	}

	size_t ConstantInt::getUnsignedValue()
	{
		return this->value;
	}




	ConstantFP* ConstantFP::get(Type* type, float val)
	{
		iceAssert(type->isFloatingPointType() && "not floating point type");
		return new ConstantFP(type, val);
	}

	ConstantFP* ConstantFP::get(Type* type, double val)
	{
		iceAssert(type->isFloatingPointType() && "not floating point type");
		return new ConstantFP(type, val);
	}

	ConstantFP::ConstantFP(Type* type, float val) : fir::ConstantValue(type)
	{
		this->value = val;
	}

	ConstantFP::ConstantFP(Type* type, double val) : fir::ConstantValue(type)
	{
		this->value = val;
	}

	double ConstantFP::getValue()
	{
		return this->value;
	}



	ConstantArray* ConstantArray::get(Type* type, std::vector<ConstantValue*> vals)
	{
		return new ConstantArray(type, vals);
	}

	ConstantArray::ConstantArray(Type* type, std::vector<ConstantValue*> vals) : fir::ConstantValue(type)
	{
		this->values = vals;
	}
}






















