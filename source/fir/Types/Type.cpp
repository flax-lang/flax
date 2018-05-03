// Type.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "errors.h"
#include "ir/type.h"

namespace pts
{
	std::string unwrapPointerType(const std::string&, int*);
}

namespace fir
{
	#if 0
	Type* FTContext::normaliseType(Type* type)
	{
		if(Type::areTypesEqual(type, this->voidType))
			return this->voidType;

		if(Type::areTypesEqual(type, this->nullType))
			return this->nullType;

		// pointer types are guaranteed to be unique due to internal caching
		// see getPointerTo
		if(type->isPointerType())
			return type;

		// find in the list.
		// todo: make this more efficient
		for(auto t : typeCache)
		{
			if(Type::areTypesEqual(t, type))
				return t;
		}

		typeCache.push_back(type);
		return type;
	}

	void FTContext::dumpTypeIDs()
	{
		for(auto t : typeCache)
			debuglog("%zu: '%s'\n", t->getID(), t);

		debuglog("\n\n");
	}


	static FTContext* defaultFTContext = 0;
	void setDefaultFTContext()
	{
		iceAssert(tc && "Tried to set null type context as default");
		defaultFTContext = tc;
	}

	FTContext* getDefaultFTContext()
	{
		if(defaultFTContext == 0)
		{
			// iceAssert(0);
			defaultFTContext = createFTContext();
		}

		return defaultFTContext;
	}


	FTContext* createFTContext()
	{
		 = new FTContext();

		// fill in primitives

		// special things
		tc->voidType = new VoidType();
		tc->nullType = new NullType();
		tc->boolType = new BoolType();

		// int8
		{
			PrimitiveType* t = new PrimitiveType(8, PrimitiveType::Kind::Integer);
			t->isTypeSigned = true;

			tc->primitiveTypes[8].push_back(t);
			tc->typeCache.push_back(t);
		}
		// int16
		{
			PrimitiveType* t = new PrimitiveType(16, PrimitiveType::Kind::Integer);
			t->isTypeSigned = true;

			tc->primitiveTypes[16].push_back(t);
			tc->typeCache.push_back(t);
		}
		// int32
		{
			PrimitiveType* t = new PrimitiveType(32, PrimitiveType::Kind::Integer);
			t->isTypeSigned = true;

			tc->primitiveTypes[32].push_back(t);
			tc->typeCache.push_back(t);
		}
		// int64
		{
			PrimitiveType* t = new PrimitiveType(64, PrimitiveType::Kind::Integer);
			t->isTypeSigned = true;

			tc->primitiveTypes[64].push_back(t);
			tc->typeCache.push_back(t);
		}
		// int128
		{
			PrimitiveType* t = new PrimitiveType(128, PrimitiveType::Kind::Integer);
			t->isTypeSigned = true;

			tc->primitiveTypes[128].push_back(t);
			tc->typeCache.push_back(t);
		}




		// uint8
		{
			PrimitiveType* t = new PrimitiveType(8, PrimitiveType::Kind::Integer);
			t->isTypeSigned = false;

			tc->primitiveTypes[8].push_back(t);
			tc->typeCache.push_back(t);
		}
		// uint16
		{
			PrimitiveType* t = new PrimitiveType(16, PrimitiveType::Kind::Integer);
			t->isTypeSigned = false;

			tc->primitiveTypes[16].push_back(t);
			tc->typeCache.push_back(t);
		}
		// uint32
		{
			PrimitiveType* t = new PrimitiveType(32, PrimitiveType::Kind::Integer);
			t->isTypeSigned = false;

			tc->primitiveTypes[32].push_back(t);
			tc->typeCache.push_back(t);
		}
		// uint64
		{
			PrimitiveType* t = new PrimitiveType(64, PrimitiveType::Kind::Integer);
			t->isTypeSigned = false;

			tc->primitiveTypes[64].push_back(t);
			tc->typeCache.push_back(t);
		}
		// uint128
		{
			PrimitiveType* t = new PrimitiveType(128, PrimitiveType::Kind::Integer);
			t->isTypeSigned = false;

			tc->primitiveTypes[128].push_back(t);
			tc->typeCache.push_back(t);
		}




		// float32
		{
			PrimitiveType* t = new PrimitiveType(32, PrimitiveType::Kind::Floating);
			t->isTypeSigned = false;

			tc->primitiveTypes[32].push_back(t);
			tc->typeCache.push_back(t);
		}
		// float64
		{
			PrimitiveType* t = new PrimitiveType(64, PrimitiveType::Kind::Floating);
			t->isTypeSigned = false;

			tc->primitiveTypes[64].push_back(t);
			tc->typeCache.push_back(t);
		}
		// float80
		{
			PrimitiveType* t = new PrimitiveType(80, PrimitiveType::Kind::Floating);
			t->isTypeSigned = false;

			tc->primitiveTypes[80].push_back(t);
			tc->typeCache.push_back(t);
		}
		// float128
		{
			PrimitiveType* t = new PrimitiveType(128, PrimitiveType::Kind::Floating);
			t->isTypeSigned = false;

			tc->primitiveTypes[128].push_back(t);
			tc->typeCache.push_back(t);
		}

		// get 'nice' IDs for the common types
		fir::Type::getAny();
		fir::Type::getRange();
		fir::Type::getString();

		fir::Type::getInt8Ptr();
		fir::Type::getInt16Ptr();
		fir::Type::getInt32Ptr();
		fir::Type::getInt64Ptr();

		fir::Type::getUint8Ptr();
		fir::Type::getUint16Ptr();
		fir::Type::getUint32Ptr();
		fir::Type::getUint64Ptr();

		return tc;
	}
	#endif





	std::string Type::typeListToString(const std::initializer_list<Type*>& types)
	{
		return typeListToString(std::vector<Type*>(types.begin(), types.end()));
	}

	std::string Type::typeListToString(const std::vector<Type*>& types)
	{
		// print types
		std::string str = "{ ";
		for(auto t : types)
			str += t->str() + ", ";

		if(str.length() > 2)
			str = str.substr(0, str.length() - 2);

		return str + " }";
	}


	bool Type::areTypeListsEqual(const std::vector<Type*>& a, const std::vector<Type*>& b)
	{
		if(a.size() != b.size()) return false;
		if(a.size() == 0 && b.size() == 0) return true;

		for(size_t i = 0; i < a.size(); i++)
		{
			if(a[i] != b[i])
				return false;
		}

		return true;
	}

	bool Type::areTypeListsEqual(const std::initializer_list<Type*>& a, const std::initializer_list<Type*>& b)
	{
		return areTypeListsEqual(std::vector<Type*>(a.begin(), a.end()), std::vector<Type*>(b.begin(), b.end()));
	}



	Type* Type::getPointerTo()
	{
		// cache the pointer internally
		if(!this->pointerTo)
		{
			PointerType* newType = new PointerType(this, false);
			this->pointerTo = newType;
		}

		return this->pointerTo;
	}

	Type* Type::getMutablePointerTo()
	{
		// cache the pointer internally
		if(!this->mutablePointerTo)
		{
			PointerType* newType = new PointerType(this, true);
			this->mutablePointerTo = newType;
		}

		return this->mutablePointerTo;
	}


	Type* Type::getMutablePointerVersion()
	{
		iceAssert(this->isPointerType() && "not pointer type");
		return this->toPointerType()->getMutable();
	}

	Type* Type::getImmutablePointerVersion()
	{
		iceAssert(this->isPointerType() && "not pointer type");
		return this->toPointerType()->getImmutable();
	}


	Type* Type::getPointerElementType()
	{
		if(!this->isPointerType())
			error("type is not a pointer ('%s')", this);


		PointerType* ptrthis = dynamic_cast<PointerType*>(this);
		iceAssert(ptrthis);

		Type* newType = ptrthis->baseType;

		// ptrthis could only have been obtained by calling getPointerTo
		// on an already normalised type, so this should not be needed
		// newType = tc->normaliseType(newType);

		return newType;
	}

	// bool Type::areTypesEqual(Type* a, Type* b)
	// {
	// 	return a->isTypeEqual(b);
	// }



	Type* Type::getIndirectedType(int times)
	{
		Type* ret = this;
		if(times > 0)
		{
			for(int i = 0; i < times; i++)
				ret = ret->getPointerTo();
		}
		else if(times < 0)
		{
			for(int i = 0; i < -times; i++)
				ret = ret->getPointerElementType();
		}
		// both getPointerTo and getPointerElementType should already
		// return normalised types
		// ret = tc->normaliseType(ret);
		return ret;
	}


	Type* Type::fromBuiltin(const std::string& builtin)
	{

		int indirections = 0;
		auto copy = pts::unwrapPointerType(builtin, &indirections);

		Type* real = 0;

		if(copy == INT8_TYPE_STRING)                    real = Type::getInt8();
		else if(copy == INT16_TYPE_STRING)              real = Type::getInt16();
		else if(copy == INT32_TYPE_STRING)              real = Type::getInt32();
		else if(copy == INT64_TYPE_STRING)              real = Type::getInt64();
		else if(copy == INT128_TYPE_STRING)             real = Type::getInt128();

		else if(copy == UINT8_TYPE_STRING)              real = Type::getUint8();
		else if(copy == UINT16_TYPE_STRING)             real = Type::getUint16();
		else if(copy == UINT32_TYPE_STRING)             real = Type::getUint32();
		else if(copy == UINT64_TYPE_STRING)             real = Type::getUint64();
		else if(copy == UINT128_TYPE_STRING)            real = Type::getUint128();

		else if(copy == FLOAT32_TYPE_STRING)            real = Type::getFloat32();
		else if(copy == FLOAT64_TYPE_STRING)            real = Type::getFloat64();
		else if(copy == FLOAT80_TYPE_STRING)            real = Type::getFloat80();
		else if(copy == FLOAT128_TYPE_STRING)           real = Type::getFloat128();

		else if(copy == STRING_TYPE_STRING)             real = Type::getString();

		else if(copy == CHARACTER_SLICE_TYPE_STRING)    real = ArraySliceType::get(Type::getInt8(), false);

		else if(copy == BOOL_TYPE_STRING)               real = Type::getBool();
		else if(copy == VOID_TYPE_STRING)               real = Type::getVoid();

		// unspecified things
		else if(copy == INTUNSPEC_TYPE_STRING)          real = Type::getInt64();
		else if(copy == UINTUNSPEC_TYPE_STRING)         real = Type::getUint64();

		else if(copy == FLOAT_TYPE_STRING)              real = Type::getFloat32();
		else if(copy == DOUBLE_TYPE_STRING)             real = Type::getFloat64();

		else if(copy == ANY_TYPE_STRING)                real = Type::getAny();

		else return 0;

		iceAssert(real);

		real = real->getIndirectedType(indirections);
		return real;
	}



	Type* Type::getArrayElementType()
	{
		if(this->isDynamicArrayType())		return this->toDynamicArrayType()->getElementType();
		else if(this->isArrayType())		return this->toArrayType()->getElementType();
		else if(this->isArraySliceType())	return this->toArraySliceType()->getElementType();
		else								error("'%s' is not an array type", this);
	}








	bool Type::isPointerTo(Type* other)
	{
		return other->getPointerTo() == this;
	}

	bool Type::isPointerElementOf(Type* other)
	{
		return this->getPointerTo() == other;
	}

	PrimitiveType* Type::toPrimitiveType()
	{
		auto t = dynamic_cast<PrimitiveType*>(this);
		if(t == 0) error("not primitive type");
		return t;
	}

	FunctionType* Type::toFunctionType()
	{
		auto t = dynamic_cast<FunctionType*>(this);
		if(t == 0) error("not function type");
		return t;
	}

	PointerType* Type::toPointerType()
	{
		auto t = dynamic_cast<PointerType*>(this);
		if(t == 0) error("not pointer type");
		return t;
	}

	StructType* Type::toStructType()
	{
		auto t = dynamic_cast<StructType*>(this);
		if(t == 0) error("not struct type");
		return t;
	}

	ClassType* Type::toClassType()
	{
		auto t = dynamic_cast<ClassType*>(this);
		if(t == 0) error("not class type");
		return t;
	}

	TupleType* Type::toTupleType()
	{
		auto t = dynamic_cast<TupleType*>(this);
		if(t == 0) error("not tuple type");
		return t;
	}

	ArrayType* Type::toArrayType()
	{
		auto t = dynamic_cast<ArrayType*>(this);
		if(t == 0) error("not array type");
		return t;
	}

	DynamicArrayType* Type::toDynamicArrayType()
	{
		auto t = dynamic_cast<DynamicArrayType*>(this);
		if(t == 0) error("not dynamic array type");
		return t;
	}

	ArraySliceType* Type::toArraySliceType()
	{
		auto t = dynamic_cast<ArraySliceType*>(this);
		if(t == 0) error("not array slice type");
		return t;
	}

	RangeType* Type::toRangeType()
	{
		auto t = dynamic_cast<RangeType*>(this);
		if(t == 0) error("not range type");
		return t;
	}

	StringType* Type::toStringType()
	{
		auto t = dynamic_cast<StringType*>(this);
		if(t == 0) error("not string type");
		return t;
	}

	EnumType* Type::toEnumType()
	{
		auto t = dynamic_cast<EnumType*>(this);
		if(t == 0) error("not enum type");
		return t;
	}

	AnyType* Type::toAnyType()
	{
		auto t = dynamic_cast<AnyType*>(this);
		if(t == 0) error("not any type");
		return t;
	}

	NullType* Type::toNullType()
	{
		auto t = dynamic_cast<NullType*>(this);
		if(t == 0) error("not null type");
		return t;
	}

	ConstantNumberType* Type::toConstantNumberType()
	{
		auto t = dynamic_cast<ConstantNumberType*>(this);
		if(t == 0) error("not constant number type");
		return t;
	}


	size_t Type::getBitWidth()
	{
		if(this->isIntegerType())
			return this->toPrimitiveType()->getIntegerBitWidth();

		else if(this->isFloatingPointType())
			return this->toPrimitiveType()->getFloatingPointBitWidth();

		else if(this->isPointerType())
			return sizeof(void*) * CHAR_BIT;

		else
			return 0;
	}












	bool Type::isConstantNumberType()
	{
		return dynamic_cast<ConstantNumberType*>(this) != 0;
	}

	bool Type::isStructType()
	{
		return dynamic_cast<StructType*>(this) != 0;
	}

	bool Type::isTupleType()
	{
		return dynamic_cast<TupleType*>(this) != 0;
	}

	bool Type::isClassType()
	{
		return dynamic_cast<ClassType*>(this) != 0;
	}

	bool Type::isPackedStruct()
	{
		return this->isStructType()
			&& (this->toStructType()->isTypePacked);
	}

	bool Type::isArrayType()
	{
		return dynamic_cast<ArrayType*>(this) != 0;
	}

	bool Type::isFloatingPointType()
	{
		auto t = dynamic_cast<PrimitiveType*>(this);
		return t != 0 && t->primKind == PrimitiveType::Kind::Floating;
	}

	bool Type::isIntegerType()
	{
		auto t = dynamic_cast<PrimitiveType*>(this);
		return t != 0 && t->primKind == PrimitiveType::Kind::Integer;
	}

	bool Type::isSignedIntType()
	{
		auto t = dynamic_cast<PrimitiveType*>(this);
		return t != 0 && t->primKind == PrimitiveType::Kind::Integer && t->isSigned();
	}

	bool Type::isFunctionType()
	{
		return dynamic_cast<FunctionType*>(this) != 0;
	}

	bool Type::isPrimitiveType()
	{
		return dynamic_cast<PrimitiveType*>(this) != 0;
	}

	bool Type::isPointerType()
	{
		return dynamic_cast<PointerType*>(this);
	}

	bool Type::isVoidType()
	{
		return dynamic_cast<VoidType*>(this) != 0;
	}

	bool Type::isDynamicArrayType()
	{
		return dynamic_cast<DynamicArrayType*>(this) != 0;
	}

	bool Type::isVariadicArrayType()
	{
		return this->isDynamicArrayType() && this->toDynamicArrayType()->isFunctionVariadic();
	}

	bool Type::isArraySliceType()
	{
		return dynamic_cast<ArraySliceType*>(this) != 0;
	}

	bool Type::isRangeType()
	{
		return dynamic_cast<RangeType*>(this) != 0;
	}

	bool Type::isCharType()
	{
		return this == fir::Type::getInt8();
	}

	bool Type::isStringType()
	{
		return dynamic_cast<StringType*>(this) != 0;
	}

	bool Type::isEnumType()
	{
		return dynamic_cast<EnumType*>(this) != 0;
	}

	bool Type::isAnyType()
	{
		return dynamic_cast<AnyType*>(this) != 0;
	}

	bool Type::isNullType()
	{
		return dynamic_cast<NullType*>(this) != 0;
	}

	bool Type::isBoolType()
	{
		return this == fir::Type::getBool();
	}

	bool Type::isMutablePointer()
	{
		return this->isPointerType() && this->toPointerType()->isMutable();
	}

	bool Type::isImmutablePointer()
	{
		return this->isPointerType() && !this->toPointerType()->isMutable();
	}

	bool Type::isCharSliceType()
	{
		return this->isArraySliceType() && this->getArrayElementType() == fir::Type::getInt8();
	}





	// static getting functions
	VoidType* Type::getVoid()
	{
		return VoidType::get();
	}

	NullType* Type::getNull()
	{
		return NullType::get();
	}

	Type* Type::getVoidPtr()
	{
		return VoidType::get()->getPointerTo();
	}

	BoolType* Type::getBool()
	{
		return BoolType::get();
	}

	PrimitiveType* Type::getInt8()
	{
		return PrimitiveType::getInt8();
	}

	PrimitiveType* Type::getInt16()
	{
		return PrimitiveType::getInt16();
	}

	PrimitiveType* Type::getInt32()
	{
		return PrimitiveType::getInt32();
	}

	PrimitiveType* Type::getInt64()
	{
		return PrimitiveType::getInt64();
	}

	PrimitiveType* Type::getInt128()
	{
		return PrimitiveType::getInt128();
	}

	PrimitiveType* Type::getUint8()
	{
		return PrimitiveType::getUint8();
	}

	PrimitiveType* Type::getUint16()
	{
		return PrimitiveType::getUint16();
	}

	PrimitiveType* Type::getUint32()
	{
		return PrimitiveType::getUint32();
	}

	PrimitiveType* Type::getUint64()
	{
		return PrimitiveType::getUint64();
	}

	PrimitiveType* Type::getUint128()
	{
		return PrimitiveType::getUint128();
	}

	PrimitiveType* Type::getFloat32()
	{
		return PrimitiveType::getFloat32();
	}

	PrimitiveType* Type::getFloat64()
	{
		return PrimitiveType::getFloat64();
	}

	PrimitiveType* Type::getFloat80()
	{
		return PrimitiveType::getFloat80();
	}

	PrimitiveType* Type::getFloat128()
	{
		return PrimitiveType::getFloat128();
	}


	PointerType* Type::getInt8Ptr()
	{
		return PointerType::getInt8Ptr();
	}

	PointerType* Type::getInt16Ptr()
	{
		return PointerType::getInt16Ptr();
	}

	PointerType* Type::getInt32Ptr()
	{
		return PointerType::getInt32Ptr();
	}

	PointerType* Type::getInt64Ptr()
	{
		return PointerType::getInt64Ptr();
	}

	PointerType* Type::getInt128Ptr()
	{
		return PointerType::getInt128Ptr();
	}

	PointerType* Type::getUint8Ptr()
	{
		return PointerType::getUint8Ptr();
	}

	PointerType* Type::getUint16Ptr()
	{
		return PointerType::getUint16Ptr();
	}

	PointerType* Type::getUint32Ptr()
	{
		return PointerType::getUint32Ptr();
	}

	PointerType* Type::getUint64Ptr()
	{
		return PointerType::getUint64Ptr();
	}

	PointerType* Type::getUint128Ptr()
	{
		return PointerType::getUint128Ptr();
	}




	PointerType* Type::getMutInt8Ptr()
	{
		return PointerType::getInt8Ptr()->getMutable();
	}

	PointerType* Type::getMutInt16Ptr()
	{
		return PointerType::getInt16Ptr()->getMutable();
	}

	PointerType* Type::getMutInt32Ptr()
	{
		return PointerType::getInt32Ptr()->getMutable();
	}

	PointerType* Type::getMutInt64Ptr()
	{
		return PointerType::getInt64Ptr()->getMutable();
	}

	PointerType* Type::getMutInt128Ptr()
	{
		return PointerType::getInt128Ptr()->getMutable();
	}

	PointerType* Type::getMutUint8Ptr()
	{
		return PointerType::getUint8Ptr()->getMutable();
	}

	PointerType* Type::getMutUint16Ptr()
	{
		return PointerType::getUint16Ptr()->getMutable();
	}

	PointerType* Type::getMutUint32Ptr()
	{
		return PointerType::getUint32Ptr()->getMutable();
	}

	PointerType* Type::getMutUint64Ptr()
	{
		return PointerType::getUint64Ptr()->getMutable();
	}

	PointerType* Type::getMutUint128Ptr()
	{
		return PointerType::getUint128Ptr()->getMutable();
	}





	ArraySliceType* Type::getCharSlice(bool mut)
	{
		return ArraySliceType::get(fir::Type::getInt8(), mut);
	}

	RangeType* Type::getRange()
	{
		return RangeType::get();
	}

	StringType* Type::getString()
	{
		return StringType::get();
	}

	AnyType* Type::getAny()
	{
		return AnyType::get();
	}

	ConstantNumberType* Type::getConstantNumber(mpfr::mpreal n)
	{
		return ConstantNumberType::get(n);
	}



}



namespace tinyformat
{
	void formatValue(std::ostream& out, const char* /*fmtBegin*/, const char* fmtEnd, int ntrunc, fir::Type* ty)
	{
		out << ty->str();
	}
}












