// ConstantValue.cpp
// Copyright (c) 2014 - 2016, zhiayang
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
		this->kind = Kind::literal;
	}

	ConstantValue* ConstantValue::getZeroValue(Type* type)
	{
		iceAssert(!type->isVoidType() && "cannot make void constant");

		return new ConstantValue(type);
	}

	ConstantValue* ConstantValue::getNull()
	{
		auto ret = new ConstantValue(fir::Type::getNull());
		return ret;
	}


	ConstantBool* ConstantBool::get(bool val)
	{
		return new ConstantBool(val);
	}

	ConstantBool::ConstantBool(bool v) : ConstantValue(fir::Type::getBool()), value(v)
	{
	}

	bool ConstantBool::getValue()
	{
		return this->value;
	}


	ConstantBitcast* ConstantBitcast::get(ConstantValue* v, Type* t)
	{
		return new ConstantBitcast(v, t);
	}

	ConstantBitcast::ConstantBitcast(ConstantValue* v, Type* t) : ConstantValue(t), value(v)
	{
	}




	ConstantNumber* ConstantNumber::get(ConstantNumberType* cnt, const mpfr::mpreal& n)
	{
		return new ConstantNumber(cnt, n);
	}

	ConstantNumber::ConstantNumber(ConstantNumberType* cnt, const mpfr::mpreal& n) : ConstantValue(cnt)
	{
		this->number = n;
	}


	// todo: unique these values.
	ConstantInt* ConstantInt::get(Type* intType, uint64_t val)
	{
		iceAssert(intType->isIntegerType() && "not integer type");
		return new ConstantInt(intType, val);
	}

	ConstantInt::ConstantInt(Type* type, int64_t val) : ConstantValue(type)
	{
		this->value = val;
	}

	ConstantInt::ConstantInt(Type* type, uint64_t val) : ConstantValue(type)
	{
		this->value = val;
	}

	int64_t ConstantInt::getSignedValue()
	{
		return (int64_t) this->value;
	}

	uint64_t ConstantInt::getUnsignedValue()
	{
		return this->value;
	}

	ConstantInt* ConstantInt::getInt8(int8_t value)
	{
		return ConstantInt::get(Type::getInt8(), value);
	}

	ConstantInt* ConstantInt::getInt16(int16_t value)
	{
		return ConstantInt::get(Type::getInt16(), value);
	}

	ConstantInt* ConstantInt::getInt32(int32_t value)
	{
		return ConstantInt::get(Type::getInt32(), value);
	}

	ConstantInt* ConstantInt::getInt64(int64_t value)
	{
		return ConstantInt::get(Type::getInt64(), value);
	}

	ConstantInt* ConstantInt::getUint8(uint8_t value)
	{
		return ConstantInt::get(Type::getUint8(), value);
	}

	ConstantInt* ConstantInt::getUint16(uint16_t value)
	{
		return ConstantInt::get(Type::getUint16(), value);
	}

	ConstantInt* ConstantInt::getUint32(uint32_t value)
	{
		return ConstantInt::get(Type::getUint32(), value);
	}

	ConstantInt* ConstantInt::getUint64(uint64_t value)
	{
		return ConstantInt::get(Type::getUint64(), value);
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
		this->value = (long double) val;
	}

	ConstantFP::ConstantFP(Type* type, double val) : fir::ConstantValue(type)
	{
		this->value = (long double) val;
	}

	double ConstantFP::getValue()
	{
		return this->value;
	}

	ConstantFP* ConstantFP::getFloat32(float value)
	{
		return ConstantFP::get(Type::getFloat32(), value);
	}

	ConstantFP* ConstantFP::getFloat64(double value)
	{
		return ConstantFP::get(Type::getFloat64(), value);
	}






	ConstantStruct* ConstantStruct::get(StructType* st, std::vector<ConstantValue*> members)
	{
		return new ConstantStruct(st, members);
	}

	ConstantStruct::ConstantStruct(StructType* st, std::vector<ConstantValue*> members) : ConstantValue(st)
	{
		if(st->getElementCount() != members.size())
			error("Mismatched structs: expected %zu fields, got %zu", st->getElementCount(), members.size());

		for(size_t i = 0; i < st->getElementCount(); i++)
		{
			if(st->getElementN(i) != members[i]->getType())
			{
				error("Mismatched types in field %zu: expected '%s', got '%s'", i, st->getElementN(i), members[i]->getType());
			}
		}

		// ok
		this->members = members;
	}







	ConstantString* ConstantString::get(std::string s)
	{
		return new ConstantString(s);
	}

	ConstantString::ConstantString(std::string s) : ConstantValue(fir::Type::getCharSlice(false))
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









	ConstantEnumCase* ConstantEnumCase::get(EnumType* et, ConstantInt* index, ConstantValue* value)
	{
		return new ConstantEnumCase(et, index, value);
	}

	ConstantInt* ConstantEnumCase::getIndex()
	{
		return this->index;
	}

	ConstantValue* ConstantEnumCase::getValue()
	{
		return this->value;
	}

	// well this is stupid.
	ConstantEnumCase::ConstantEnumCase(EnumType* et, ConstantInt* index, ConstantValue* value) : fir::ConstantValue(et)
	{
		this->index = index;
		this->value = value;
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





	ConstantArraySlice* ConstantArraySlice::get(ArraySliceType* t, ConstantValue* d, ConstantValue* l)
	{
		auto cda = new ConstantArraySlice(t);
		cda->data = d;
		cda->length = l;

		return cda;
	}

	ConstantArraySlice::ConstantArraySlice(ArraySliceType* t) : ConstantValue(t)
	{
	}
}






















