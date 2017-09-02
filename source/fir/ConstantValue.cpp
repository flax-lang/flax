// ConstantValue.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ir/value.h"
#include "ir/constant.h"

#include <cmath>
#include <cfloat>
#include <functional>

namespace fir
{
	ConstantValue::ConstantValue(Type* t) : Value(t)
	{
		// nothing.
	}

	ConstantValue* ConstantValue::getZeroValue(Type* type)
	{
		iceAssert(!type->isVoidType() && "cannot make void constant");

		auto ret = new ConstantValue(type);
		return ret;
	}

	ConstantValue* ConstantValue::getNull()
	{
		auto ret = new ConstantValue(fir::Type::getVoid()->getPointerTo());
		return ret;
	}


	ConstantNumber* ConstantNumber::get(mpfr::mpreal n)
	{
		return new ConstantNumber(n);
	}

	mpfr::mpreal ConstantNumber::getValue()
	{
		return this->number;
	}

	ConstantNumber::ConstantNumber(mpfr::mpreal n) : ConstantValue(fir::Type::getConstantNumber(n))
	{
		this->number = n;
	}


	// todo: unique these values.
	ConstantInt* ConstantInt::get(Type* intType, size_t val)
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
		Type* t = Type::getBool(tc);
		return ConstantInt::get(t, value);
	}

	ConstantInt* ConstantInt::getInt8(int8_t value, FTContext* tc)
	{
		Type* t = Type::getInt8(tc);
		return ConstantInt::get(t, value);
	}

	ConstantInt* ConstantInt::getInt16(int16_t value, FTContext* tc)
	{
		Type* t = Type::getInt16(tc);
		return ConstantInt::get(t, value);
	}

	ConstantInt* ConstantInt::getInt32(int32_t value, FTContext* tc)
	{
		Type* t = Type::getInt32(tc);
		return ConstantInt::get(t, value);
	}

	ConstantInt* ConstantInt::getInt64(int64_t value, FTContext* tc)
	{
		Type* t = Type::getInt64(tc);
		return ConstantInt::get(t, value);
	}

	ConstantInt* ConstantInt::getUint8(uint8_t value, FTContext* tc)
	{
		Type* t = Type::getUint8(tc);
		return ConstantInt::get(t, value);
	}

	ConstantInt* ConstantInt::getUint16(uint16_t value, FTContext* tc)
	{
		Type* t = Type::getUint16(tc);
		return ConstantInt::get(t, value);
	}

	ConstantInt* ConstantInt::getUint32(uint32_t value, FTContext* tc)
	{
		Type* t = Type::getUint32(tc);
		return ConstantInt::get(t, value);
	}

	ConstantInt* ConstantInt::getUint64(uint64_t value, FTContext* tc)
	{
		Type* t = Type::getUint64(tc);
		return ConstantInt::get(t, value);
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

	ConstantFP* ConstantFP::get(Type* type, long double val)
	{
		iceAssert(type->isFloatingPointType() && "not floating point type");
		return new ConstantFP(type, val);
	}

	ConstantFP::ConstantFP(Type* type, float val) : fir::ConstantValue(type)
	{
		this->value = (long double) val;
	}

	ConstantFP::ConstantFP(Type* type, double val) : fir::ConstantValue(type)
	{
		this->value = (long double) val;
	}

	ConstantFP::ConstantFP(Type* type, long double val) : fir::ConstantValue(type)
	{
		this->value = val;
	}

	long double ConstantFP::getValue()
	{
		return this->value;
	}

	ConstantFP* ConstantFP::getFloat32(float value, FTContext* tc)
	{
		Type* t = Type::getFloat32(tc);
		return ConstantFP::get(t, value);
	}

	ConstantFP* ConstantFP::getFloat64(double value, FTContext* tc)
	{
		Type* t = Type::getFloat64(tc);
		return ConstantFP::get(t, value);
	}

	ConstantFP* ConstantFP::getFloat80(long double value, FTContext* tc)
	{
		Type* t = Type::getFloat80(tc);
		return ConstantFP::get(t, value);
	}






	ConstantStruct* ConstantStruct::get(StructType* st, std::vector<ConstantValue*> members)
	{
		return new ConstantStruct(st, members);
	}

	ConstantStruct::ConstantStruct(StructType* st, std::vector<ConstantValue*> members) : ConstantValue(st)
	{
		if(st->getElementCount() != members.size())
			_error_and_exit("Mismatched structs: expected %zu fields, got %zu", st->getElementCount(), members.size());

		for(size_t i = 0; i < st->getElementCount(); i++)
		{
			if(st->getElementN(i) != members[i]->getType())
			{
				_error_and_exit("Mismatched types in field %zu: expected '%s', got '%s'", i, st->getElementN(i)->str().c_str(),
					members[i]->getType()->str().c_str());
			}
		}

		// ok
		this->members = members;
	}







	ConstantString* ConstantString::get(std::string s)
	{
		return new ConstantString(s);
	}

	ConstantString::ConstantString(std::string s) : ConstantValue(fir::StringType::get())
	{
		this->str = s;
	}

	std::string ConstantString::getValue()
	{
		return this->str;
	}



	ConstantTuple* ConstantTuple::get(std::vector<ConstantValue*> mems)
	{
		return new ConstantTuple(mems);
	}

	std::vector<ConstantValue*> ConstantTuple::getValues()
	{
		return this->values;
	}

	static std::vector<Type*> mapTypes(std::vector<ConstantValue*> vs)
	{
		std::vector<Type*> ret;
		for(auto v : vs)
			ret.push_back(v->getType());

		return ret;
	}

	// well this is stupid.
	ConstantTuple::ConstantTuple(std::vector<ConstantValue*> mems) : fir::ConstantValue(fir::TupleType::get(mapTypes(mems)))
	{
		this->values = mems;
	}































	ConstantArray* ConstantArray::get(Type* type, std::vector<ConstantValue*> vals)
	{
		return new ConstantArray(type, vals);
	}

	ConstantArray::ConstantArray(Type* type, std::vector<ConstantValue*> vals) : fir::ConstantValue(type)
	{
		this->values = vals;
	}





	ConstantDynamicArray* ConstantDynamicArray::get(DynamicArrayType* t, ConstantValue* d, ConstantValue* l, ConstantValue* c)
	{
		auto cda = new ConstantDynamicArray(t);
		cda->data = d;
		cda->length = l;
		cda->capacity = c;

		return cda;
	}

	ConstantDynamicArray* ConstantDynamicArray::get(ConstantArray* arr)
	{
		auto cda = new ConstantDynamicArray(DynamicArrayType::get(arr->getType()->toArrayType()->getElementType()));
		cda->arr = arr;

		return cda;
	}


	ConstantDynamicArray::ConstantDynamicArray(DynamicArrayType* t) : ConstantValue(t)
	{
	}








	bool checkLiteralFitsIntoType(fir::PrimitiveType* target, mpfr::mpreal num)
	{
		using mpr = mpfr::mpreal;
		if(mpfr::isint(num) && target->isFloatingPointType())
		{
			if(target == fir::Type::getFloat32())		return (num >= -mpr(__FLT_MAX__) && num <= mpr(__FLT_MAX__));
			else if(target == fir::Type::getFloat64())	return (num >= -mpr(__DBL_MAX__) && num <= mpr(__DBL_MAX__));
			else if(target == fir::Type::getFloat80())	return (num >= -mpr(__LDBL_MAX__) && num <= mpr(__LDBL_MAX__));
			else										error("unsupported type '%s'", target->str());
		}
		else
		{
			if(target == fir::Type::getFloat32())		return (mpfr::abs(num) >= mpr(__FLT_MIN__) && mpfr::abs(num) <= mpr(__FLT_MAX__));
			else if(target == fir::Type::getFloat64())	return (mpfr::abs(num) >= mpr(__DBL_MIN__) && mpfr::abs(num) <= mpr(__DBL_MAX__));
			else if(target == fir::Type::getFloat80())	return (mpfr::abs(num) >= mpr(__LDBL_MIN__) && mpfr::abs(num) <= mpr(__LDBL_MAX__));

			else if(target == fir::Type::getInt8())		return (num >= mpr(INT8_MIN) && num <= mpr(INT8_MAX));
			else if(target == fir::Type::getInt16())	return (num >= mpr(INT16_MIN) && num <= mpr(INT16_MAX));
			else if(target == fir::Type::getInt32())	return (num >= mpr(INT32_MIN) && num <= mpr(INT32_MAX));
			else if(target == fir::Type::getInt64())	return (num >= mpr(INT64_MIN) && num <= mpr(INT64_MAX));
			else if(target == fir::Type::getUint8())	return (num <= mpr(UINT8_MAX));
			else if(target == fir::Type::getUint16())	return (num <= mpr(UINT16_MAX));
			else if(target == fir::Type::getUint32())	return (num <= mpr(UINT32_MAX));
			else if(target == fir::Type::getUint64())	return (num <= mpr(UINT64_MAX));
			else										error("unsupported type '%s'", target->str());
		}
	}
}






















