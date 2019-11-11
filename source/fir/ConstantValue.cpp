// ConstantValue.cpp
// Copyright (c) 2014 - 2016, zhiayang
// Licensed under the Apache License Version 2.0.

#include <math.h>
#include <float.h>
#include <inttypes.h>

#include <functional>

#include "ir/value.h"
#include "ir/constant.h"

namespace fir
{
	ConstantValue::ConstantValue(Type* t) : Value(t)
	{
		this->kind = Kind::prvalue;
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

	std::string ConstantValue::str()
	{
		return "<unknown>";
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

	std::string ConstantBool::str()
	{
		return this->value ? "true" : "false";
	}






	ConstantBitcast* ConstantBitcast::get(ConstantValue* v, Type* t)
	{
		return new ConstantBitcast(v, t);
	}

	ConstantBitcast::ConstantBitcast(ConstantValue* v, Type* t) : ConstantValue(t), value(v)
	{
	}

	std::string ConstantBitcast::str()
	{
		return this->value->str();
	}




	ConstantNumber* ConstantNumber::get(ConstantNumberType* cnt, const mpfr::mpreal& n)
	{
		return new ConstantNumber(cnt, n);
	}

	ConstantNumber::ConstantNumber(ConstantNumberType* cnt, const mpfr::mpreal& n) : ConstantValue(cnt)
	{
		this->number = n;
	}

	std::string ConstantNumber::str()
	{
		// 6 decimal places, like default printf.
		return this->number.toString("%.6R");
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
		return static_cast<int64_t>(this->value);
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

	ConstantInt* ConstantInt::getNative(int64_t value)
	{
		return ConstantInt::get(Type::getNativeWord(), value);
	}

	ConstantInt* ConstantInt::getUNative(uint64_t value)
	{
		return ConstantInt::get(Type::getNativeUWord(), value);
	}

	std::string ConstantInt::str()
	{
		char buf[64] = {0};
		if(this->getType() == Type::getInt8())          snprintf(buf, 63, "%" PRIi8,  static_cast<int8_t>(this->value));
		else if(this->getType() == Type::getInt16())    snprintf(buf, 63, "%" PRIi16, static_cast<int16_t>(this->value));
		else if(this->getType() == Type::getInt32())    snprintf(buf, 63, "%" PRIi32, static_cast<int32_t>(this->value));
		else if(this->getType() == Type::getInt64())    snprintf(buf, 63, "%" PRIi64, static_cast<int64_t>(this->value));
		else if(this->getType() == Type::getUint8())    snprintf(buf, 63, "%" PRIu8,  static_cast<uint8_t>(this->value));
		else if(this->getType() == Type::getUint16())   snprintf(buf, 63, "%" PRIu16, static_cast<uint16_t>(this->value));
		else if(this->getType() == Type::getUint32())   snprintf(buf, 63, "%" PRIu32, static_cast<uint32_t>(this->value));
		else if(this->getType() == Type::getUint64())   snprintf(buf, 63, "%" PRIu64, static_cast<uint64_t>(this->value));
		else                                            snprintf(buf, 63, "<unknown int>");

		return std::string(buf);
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
		this->value = static_cast<double>(val);
	}

	ConstantFP::ConstantFP(Type* type, double val) : fir::ConstantValue(type)
	{
		this->value = val;
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

	std::string ConstantFP::str()
	{
		char buf[64] = {0};
		if(this->getType() == Type::getFloat32())       snprintf(buf, 63, "%.6f", static_cast<float>(this->value));
		else if(this->getType() == Type::getFloat64())  snprintf(buf, 63, "%.6f", static_cast<double>(this->value));
		else                                            snprintf(buf, 63, "<unknown float>");

		return std::string(buf);
	}







	ConstantStruct* ConstantStruct::get(StructType* st, const std::vector<ConstantValue*>& members)
	{
		return new ConstantStruct(st, members);
	}

	ConstantStruct::ConstantStruct(StructType* st, const std::vector<ConstantValue*>& members) : ConstantValue(st)
	{
		if(st->getElementCount() != members.size())
			error("mismatched structs: expected %d fields, got %d", st->getElementCount(), members.size());

		for(size_t i = 0; i < st->getElementCount(); i++)
		{
			if(st->getElementN(i) != members[i]->getType())
			{
				error("mismatched types in field %d: expected '%s', got '%s'", i, st->getElementN(i), members[i]->getType());
			}
		}

		// ok
		this->members = members;
	}

	std::string ConstantStruct::str()
	{
		std::string ret = this->getType()->str() + " {\n";
		for(auto x : this->members)
			ret += "  " + x->str() + "\n";

		return ret + "}";
	}






	ConstantCharSlice* ConstantCharSlice::get(const std::string& s)
	{
		return new ConstantCharSlice(s);
	}

	ConstantCharSlice::ConstantCharSlice(const std::string& s) : ConstantValue(fir::Type::getCharSlice(false))
	{
		this->value = s;
	}

	std::string ConstantCharSlice::getValue()
	{
		return this->value;
	}

	std::string ConstantCharSlice::str()
	{
		return this->value;
	}





	ConstantTuple* ConstantTuple::get(const std::vector<ConstantValue*>& mems)
	{
		return new ConstantTuple(mems);
	}

	std::vector<ConstantValue*> ConstantTuple::getValues()
	{
		return this->values;
	}

	static std::vector<Type*> mapTypes(const std::vector<ConstantValue*>& vs)
	{
		std::vector<Type*> ret;
		ret.reserve(vs.size());

		for(auto v : vs)
			ret.push_back(v->getType());

		return ret;
	}

	// well this is stupid.
	ConstantTuple::ConstantTuple(const std::vector<ConstantValue*>& mems) : fir::ConstantValue(fir::TupleType::get(mapTypes(mems)))
	{
		this->values = mems;
	}

	std::string ConstantTuple::str()
	{
		return "(" + util::listToString(this->values, [](auto x) -> auto { return x->str(); }) + ")";
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

	std::string ConstantEnumCase::str()
	{
		// TODO: why the fuck did i design enums this way?!
		return this->value->str();
	}
























	ConstantArray* ConstantArray::get(Type* type, const std::vector<ConstantValue*>& vals)
	{
		return new ConstantArray(type, vals);
	}

	ConstantArray::ConstantArray(Type* type, const std::vector<ConstantValue*>& vals) : fir::ConstantValue(type)
	{
		this->values = vals;
	}

	std::string ConstantArray::str()
	{
		return "[ " + util::listToString(this->values, [](auto x) -> auto { return x->str(); }) + " ]";
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

	std::string ConstantDynamicArray::str()
	{
		return "<dyn array>";
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

	std::string ConstantArraySlice::str()
	{
		return "<slice>";
	}
}






















