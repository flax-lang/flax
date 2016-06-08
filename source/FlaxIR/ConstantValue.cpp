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
		auto ret = new ConstantValue(type);
		return ret;
	}

	ConstantValue* ConstantValue::getNull()
	{
		auto ret = new ConstantValue(fir::PrimitiveType::getVoid()->getPointerTo());
		return ret;
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

	ConstantInt* ConstantInt::getBool(bool value, FTContext* tc)
	{
		Type* t = PrimitiveType::getBool(tc);
		return ConstantInt::getSigned(t, value);
	}

	ConstantInt* ConstantInt::getInt8(int8_t value, FTContext* tc)
	{
		Type* t = PrimitiveType::getInt8(tc);
		return ConstantInt::getSigned(t, value);
	}

	ConstantInt* ConstantInt::getInt16(int16_t value, FTContext* tc)
	{
		Type* t = PrimitiveType::getInt16(tc);
		return ConstantInt::getSigned(t, value);
	}

	ConstantInt* ConstantInt::getInt32(int32_t value, FTContext* tc)
	{
		Type* t = PrimitiveType::getInt32(tc);
		return ConstantInt::getSigned(t, value);
	}

	ConstantInt* ConstantInt::getInt64(int64_t value, FTContext* tc)
	{
		Type* t = PrimitiveType::getInt64(tc);
		return ConstantInt::getSigned(t, value);
	}

	ConstantInt* ConstantInt::getUint8(uint8_t value, FTContext* tc)
	{
		Type* t = PrimitiveType::getUint8(tc);
		return ConstantInt::getUnsigned(t, value);
	}

	ConstantInt* ConstantInt::getUint16(uint16_t value, FTContext* tc)
	{
		Type* t = PrimitiveType::getUint16(tc);
		return ConstantInt::getUnsigned(t, value);
	}

	ConstantInt* ConstantInt::getUint32(uint32_t value, FTContext* tc)
	{
		Type* t = PrimitiveType::getUint32(tc);
		return ConstantInt::getUnsigned(t, value);
	}

	ConstantInt* ConstantInt::getUint64(uint64_t value, FTContext* tc)
	{
		Type* t = PrimitiveType::getUint64(tc);
		return ConstantInt::getUnsigned(t, value);
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

	ConstantFP* ConstantFP::getFloat32(float value, FTContext* tc)
	{
		Type* t = PrimitiveType::getFloat32(tc);
		return ConstantFP::get(t, value);
	}

	ConstantFP* ConstantFP::getFloat64(double value, FTContext* tc)
	{
		Type* t = PrimitiveType::getFloat64(tc);
		return ConstantFP::get(t, value);
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






















