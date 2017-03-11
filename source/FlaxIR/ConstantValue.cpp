// ConstantValue.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ir/value.h"
#include "ir/constant.h"

#include <cmath>
#include <cfloat>

namespace fir
{
	ConstantValue::ConstantValue(Type* t) : Value(t)
	{
		// nothing.
	}

	ConstantValue* ConstantValue::getNullValue(Type* type)
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



















	ConstantValue* createConstantValueCast(ConstantValue* cv, fir::Type* type)
	{
		if(dynamic_cast<ConstantInt*>(cv) || dynamic_cast<ConstantFP*>(cv))
		{
			if(!type->isFloatingPointType() && !type->isIntegerType())
			{
				die:
				error("Invaild constant cast from type '%s' to '%s'", cv->getType()->str().c_str(), type->str().c_str());
			}

			if(ConstantFP* cf = dynamic_cast<ConstantFP*>(cv))
			{
				// floats can only be casted to floats
				if(!type->isFloatingPointType()) goto die;

				// ok, make a float constant
				bool r = checkFloatingPointLiteralFitsIntoType(type->toPrimitiveType(), cf->getValue());
				if(!r) error("Constant value '%Lf' cannot fit into type '%s'", cf->getValue(), type->str().c_str());

				return ConstantFP::get(type, cf->getValue());
			}
			else
			{
				ConstantInt* ci = dynamic_cast<ConstantInt*>(cv);

				if(type->isFloatingPointType())
				{
					// ok, int-to-float
					if(ci->getType()->isSignedIntType())
					{
						bool r = checkFloatingPointLiteralFitsIntoType(type->toPrimitiveType(), (long double) ci->getSignedValue());
						if(!r) error("Constant value '%zd' cannot fit into type '%s'", ci->getSignedValue(), type->str().c_str());

						return ConstantFP::get(type, (long double) ci->getSignedValue());
					}
					else
					{
						bool r = checkFloatingPointLiteralFitsIntoType(type->toPrimitiveType(), (long double) ci->getUnsignedValue());
						if(!r) error("Constant value '%zu' cannot fit into type '%s'", ci->getUnsignedValue(), type->str().c_str());

						return ConstantFP::get(type, (long double) ci->getUnsignedValue());
					}
				}
				else
				{
					// int-to-int.
					if(ci->getType()->isSignedIntType())
					{
						bool r = checkSignedIntLiteralFitsIntoType(type->toPrimitiveType(), ci->getSignedValue());
						if(!r) error("Constant value '%zd' cannot fit into type '%s'", ci->getSignedValue(), type->str().c_str());

						return ConstantInt::get(type, ci->getSignedValue());
					}
					else
					{
						bool r = checkUnsignedIntLiteralFitsIntoType(type->toPrimitiveType(), ci->getUnsignedValue());
						if(!r) error("Constant value '%zu' cannot fit into type '%s'", ci->getUnsignedValue(), type->str().c_str());

						return ConstantInt::get(type, ci->getUnsignedValue());
					}
				}
			}
		}
		// else if(ConstantChar* cc = dynamic_cast<ConstantChar*>(cv))
		// {
		// }
		// else if(ConstantArray* ca = dynamic_cast<ConstantArray*>(cv))
		// {
		// }
		else
		{
			error("not supported");
		}
	}























	bool checkSignedIntLiteralFitsIntoType(fir::PrimitiveType* type, ssize_t val)
	{
		iceAssert(type->isIntegerType());
		if(type->isSigned())
		{
			ssize_t max = ((size_t) 1 << (type->getIntegerBitWidth() - 1)) - 1;
			ssize_t min = -max - 1;

			return val <= max && val >= min;
		}
		else
		{
			size_t max = 2ULL * ((1ULL << (type->getIntegerBitWidth() - 1)) - 1) + 1ULL;

			// switch(type->getIntegerBitWidth())
			// {
			// 	case 8: 	max = UINT8_MAX; break;
			// 	case 16:	max = UINT16_MAX; break;
			// 	case 32:	max = UINT32_MAX; break;
			// 	case 64:	max = UINT64_MAX; break;
			// 	default:	iceAssert(0);
			// }

			// won't get overflow problems, because short-circuiting makes sure val is positive.
			return val >= 0 && (size_t) val <= max;
		}
	}

	bool checkUnsignedIntLiteralFitsIntoType(fir::PrimitiveType* type, size_t val)
	{
		iceAssert(type->isIntegerType());
		size_t max = 0;

		if(type->isSigned())
		{
			max = (1 << (type->getIntegerBitWidth() - 1)) - 1;
			return val <= max;
		}
		else
		{
			size_t max = 2ULL * ((1ULL << (type->getIntegerBitWidth() - 1)) - 1) + 1ULL;
			// switch(type->getIntegerBitWidth())
			// {
			// 	case 8: 	max = UINT8_MAX; break;
			// 	case 16:	max = UINT16_MAX; break;
			// 	case 32:	max = UINT32_MAX; break;
			// 	case 64:	max = UINT64_MAX; break;
			// 	default:	iceAssert(0);
			// }

			return val <= max;
		}
	}



	// #define PRECISION_CHECK(x,eps)	(std::fabs((long double) ((double) (x)) - x) < (long double) (eps))
	#define PRECISION_CHECK(x,eps)		(true)


	bool checkFloatingPointLiteralFitsIntoType(fir::PrimitiveType* type, long double val)
	{
		if(type->getFloatingPointBitWidth() == 32)
		{
			if(val != 0.0L && (std::fabs(val) < (long double) FLT_MIN || std::fabs(val) > (long double) FLT_MAX))
				return false;

			return PRECISION_CHECK(val, FLT_EPSILON);
		}
		else if(type->getFloatingPointBitWidth() == 64)
		{
			if(val != 0.0L && (std::fabs(val) < (long double) DBL_MIN || std::fabs(val) > (long double) DBL_MAX))
				return false;

			return PRECISION_CHECK(val, DBL_EPSILON);
		}

		return true;
		// else if(type->getFloatingPointBitWidth() > 64)
		// {
		// 	if(val < LDBL_MIN || val > LDBL_MAX)
		// 		return false;

		// 	return true;
		// }
	}
}






















