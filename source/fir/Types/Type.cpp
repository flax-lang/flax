// Type.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "errors.h"
#include "ir/type.h"

namespace pts
{
	std::string unwrapPointerType(std::string, int*);
}

namespace fir
{
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
			printf("%zu: %s\n", t->getID(), t->str().c_str());

		printf("\n\n");
	}


	static FTContext* defaultFTContext = 0;
	void setDefaultFTContext(FTContext* tc)
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
		FTContext* tc = new FTContext();

		// fill in primitives

		// special things
		tc->voidType = new VoidType();
		tc->nullType = new NullType();

		tc->constantSIntType = new PrimitiveType(0, PrimitiveType::Kind::ConstantInt);
		tc->constantSIntType->isTypeSigned = true;

		tc->constantUIntType = new PrimitiveType(0, PrimitiveType::Kind::ConstantInt);
		tc->constantUIntType->isTypeSigned = false;

		tc->constantFloatType = new PrimitiveType(0, PrimitiveType::Kind::ConstantFloat);

		// bool
		{
			PrimitiveType* t = new PrimitiveType(1, PrimitiveType::Kind::Integer);
			t->isTypeSigned = false;

			tc->primitiveTypes[1].push_back(t);
			tc->typeCache.push_back(t);
		}


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
		fir::Type::getAnyType(tc);
		fir::Type::getRangeType(tc);
		fir::Type::getCharType(tc);
		fir::Type::getStringType(tc);

		fir::Type::getInt8Ptr(tc);
		fir::Type::getInt16Ptr(tc);
		fir::Type::getInt32Ptr(tc);
		fir::Type::getInt64Ptr(tc);

		fir::Type::getUint8Ptr(tc);
		fir::Type::getUint16Ptr(tc);
		fir::Type::getUint32Ptr(tc);
		fir::Type::getUint64Ptr(tc);

		return tc;
	}






	std::string Type::typeListToString(std::initializer_list<Type*> types)
	{
		return typeListToString(std::vector<Type*>(types.begin(), types.end()));
	}

	std::string Type::typeListToString(std::vector<Type*> types)
	{
		// print types
		std::string str = "{ ";
		for(auto t : types)
			str += t->str() + ", ";

		if(str.length() > 2)
			str = str.substr(0, str.length() - 2);

		return str + " }";
	}


	bool Type::areTypeListsEqual(std::vector<Type*> a, std::vector<Type*> b)
	{
		if(a.size() != b.size()) return false;
		if(a.size() == 0 && b.size() == 0) return true;

		for(size_t i = 0; i < a.size(); i++)
		{
			if(!Type::areTypesEqual(a[i], b[i]))
				return false;
		}

		return true;
	}

	bool Type::areTypeListsEqual(std::initializer_list<Type*> a, std::initializer_list<Type*> b)
	{
		return areTypeListsEqual(std::vector<Type*>(a.begin(), a.end()), std::vector<Type*>(b.begin(), b.end()));
	}



	Type* Type::getPointerTo(FTContext* tc)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		// cache the pointer internally
		if (!pointerTo) {
			PointerType* newType = new PointerType(this);
			PointerType* normalised = dynamic_cast<PointerType*>(tc->normaliseType(newType));

			iceAssert(newType);
			// the type shouldn't be in the global cache at this point yet
			iceAssert(normalised == newType);

			pointerTo = normalised;
		}

		return pointerTo;
	}

	Type* Type::getPointerElementType(FTContext* tc)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		if(!this->isPointerType())
			error("type is not a pointer (%s)", this->str().c_str());


		PointerType* ptrthis = dynamic_cast<PointerType*>(this);
		iceAssert(ptrthis);

		Type* newType = ptrthis->baseType;
		// ptrthis could only have been obtained by calling getPointerTo
		// on an already normalised type, so this should not be needed
		// newType = tc->normaliseType(newType);

		return newType;
	}

	bool Type::areTypesEqual(Type* a, Type* b)
	{
		return a->isTypeEqual(b);
	}



	Type* Type::getIndirectedType(ssize_t times, FTContext* tc)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		Type* ret = this;
		if(times > 0)
		{
			for(ssize_t i = 0; i < times; i++)
			{
				// auto old = ret;
				ret = ret->getPointerTo();
			}
		}
		else if(times < 0)
		{
			for(ssize_t i = 0; i < -times; i++)
				ret = ret->getPointerElementType();
		}
		// both getPointerTo and getPointerElementType should already
		// return normalised types
		// ret = tc->normaliseType(ret);
		return ret;
	}


	Type* Type::fromBuiltin(std::string builtin, FTContext* tc)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");


		int indirections = 0;
		builtin = pts::unwrapPointerType(builtin, &indirections);

		Type* real = 0;

		if(builtin == INT8_TYPE_STRING)				real = Type::getInt8(tc);
		else if(builtin == INT16_TYPE_STRING)		real = Type::getInt16(tc);
		else if(builtin == INT32_TYPE_STRING)		real = Type::getInt32(tc);
		else if(builtin == INT64_TYPE_STRING)		real = Type::getInt64(tc);
		else if(builtin == INT128_TYPE_STRING)		real = Type::getInt128(tc);

		else if(builtin == UINT8_TYPE_STRING)		real = Type::getUint8(tc);
		else if(builtin == UINT16_TYPE_STRING)		real = Type::getUint16(tc);
		else if(builtin == UINT32_TYPE_STRING)		real = Type::getUint32(tc);
		else if(builtin == UINT64_TYPE_STRING)		real = Type::getUint64(tc);
		else if(builtin == UINT128_TYPE_STRING)		real = Type::getUint128(tc);

		else if(builtin == FLOAT32_TYPE_STRING)		real = Type::getFloat32(tc);
		else if(builtin == FLOAT64_TYPE_STRING)		real = Type::getFloat64(tc);
		else if(builtin == FLOAT80_TYPE_STRING)		real = Type::getFloat80(tc);
		else if(builtin == FLOAT128_TYPE_STRING)	real = Type::getFloat128(tc);

		else if(builtin == STRING_TYPE_STRING)		real = Type::getStringType();
		else if(builtin == CHARACTER_TYPE_STRING)	real = Type::getCharType();

		else if(builtin == BOOL_TYPE_STRING)		real = Type::getBool(tc);
		else if(builtin == VOID_TYPE_STRING)		real = Type::getVoid(tc);

		// unspecified things
		else if(builtin == INTUNSPEC_TYPE_STRING)	real = Type::getInt64(tc);
		else if(builtin == UINTUNSPEC_TYPE_STRING)	real = Type::getUint64(tc);

		else if(builtin == FLOAT_TYPE_STRING)		real = Type::getFloat32(tc);
		else if(builtin == DOUBLE_TYPE_STRING)		real = Type::getFloat64(tc);

		else if(builtin == ANY_TYPE_STRING)			real = Type::getAnyType(tc);

		else return 0;

		iceAssert(real);

		real = real->getIndirectedType(indirections);
		return real;
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

	CharType* Type::toCharType()
	{
		auto t = dynamic_cast<CharType*>(this);
		if(t == 0) error("not char type");
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














	bool Type::isConstantIntType()
	{
		auto cn = dynamic_cast<PrimitiveType*>(this);
		return cn && cn->primKind == PrimitiveType::Kind::ConstantInt;
	}

	bool Type::isConstantFloatType()
	{
		auto cn = dynamic_cast<PrimitiveType*>(this);
		return cn && cn->primKind == PrimitiveType::Kind::ConstantFloat;
	}

	bool Type::isConstantNumberType()
	{
		auto cn = dynamic_cast<PrimitiveType*>(this);
		return cn && (cn->primKind == PrimitiveType::Kind::ConstantFloat || cn->primKind == PrimitiveType::Kind::ConstantInt);
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
		return t != 0 && (t->primKind == PrimitiveType::Kind::Floating || t->primKind == PrimitiveType::Kind::ConstantFloat);
	}

	bool Type::isIntegerType()
	{
		auto t = dynamic_cast<PrimitiveType*>(this);
		return t != 0 && (t->primKind == PrimitiveType::Kind::Integer || t->primKind == PrimitiveType::Kind::ConstantInt);
	}

	bool Type::isSignedIntType()
	{
		auto t = dynamic_cast<PrimitiveType*>(this);
		return t != 0 && (t->primKind == PrimitiveType::Kind::Integer || t->primKind == PrimitiveType::Kind::ConstantInt) && t->isSigned();
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
		return dynamic_cast<PointerType*>(this) != 0;
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

	bool Type::isVoidPointer()
	{
		return this == PrimitiveType::getVoid()->getPointerTo();
	}

	bool Type::isRangeType()
	{
		return dynamic_cast<RangeType*>(this) != 0;
	}

	bool Type::isStringType()
	{
		return dynamic_cast<StringType*>(this) != 0;
	}

	bool Type::isCharType()
	{
		return dynamic_cast<CharType*>(this) != 0;
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











	// static conv. functions

	VoidType* Type::getVoid(FTContext* tc)
	{
		return VoidType::get(tc);
	}

	NullType* Type::getNull(FTContext* tc)
	{
		return NullType::get(tc);
	}

	Type* Type::getVoidPtr(FTContext* tc)
	{
		return VoidType::get(tc)->getPointerTo();
	}

	PrimitiveType* Type::getBool(FTContext* tc)
	{
		return PrimitiveType::getBool(tc);
	}

	PrimitiveType* Type::getInt8(FTContext* tc)
	{
		return PrimitiveType::getInt8(tc);
	}

	PrimitiveType* Type::getInt16(FTContext* tc)
	{
		return PrimitiveType::getInt16(tc);
	}

	PrimitiveType* Type::getInt32(FTContext* tc)
	{
		return PrimitiveType::getInt32(tc);
	}

	PrimitiveType* Type::getInt64(FTContext* tc)
	{
		return PrimitiveType::getInt64(tc);
	}

	PrimitiveType* Type::getInt128(FTContext* tc)
	{
		return PrimitiveType::getInt128(tc);
	}

	PrimitiveType* Type::getUint8(FTContext* tc)
	{
		return PrimitiveType::getUint8(tc);
	}

	PrimitiveType* Type::getUint16(FTContext* tc)
	{
		return PrimitiveType::getUint16(tc);
	}

	PrimitiveType* Type::getUint32(FTContext* tc)
	{
		return PrimitiveType::getUint32(tc);
	}

	PrimitiveType* Type::getUint64(FTContext* tc)
	{
		return PrimitiveType::getUint64(tc);
	}

	PrimitiveType* Type::getUint128(FTContext* tc)
	{
		return PrimitiveType::getUint128(tc);
	}

	PrimitiveType* Type::getFloat32(FTContext* tc)
	{
		return PrimitiveType::getFloat32(tc);
	}

	PrimitiveType* Type::getFloat64(FTContext* tc)
	{
		return PrimitiveType::getFloat64(tc);
	}

	PrimitiveType* Type::getFloat80(FTContext* tc)
	{
		return PrimitiveType::getFloat80(tc);
	}

	PrimitiveType* Type::getFloat128(FTContext* tc)
	{
		return PrimitiveType::getFloat128(tc);
	}


	PointerType* Type::getInt8Ptr(FTContext* tc)
	{
		return PointerType::getInt8Ptr(tc);
	}

	PointerType* Type::getInt16Ptr(FTContext* tc)
	{
		return PointerType::getInt16Ptr(tc);
	}

	PointerType* Type::getInt32Ptr(FTContext* tc)
	{
		return PointerType::getInt32Ptr(tc);
	}

	PointerType* Type::getInt64Ptr(FTContext* tc)
	{
		return PointerType::getInt64Ptr(tc);
	}

	PointerType* Type::getInt128Ptr(FTContext* tc)
	{
		return PointerType::getInt128Ptr(tc);
	}

	PointerType* Type::getUint8Ptr(FTContext* tc)
	{
		return PointerType::getUint8Ptr(tc);
	}

	PointerType* Type::getUint16Ptr(FTContext* tc)
	{
		return PointerType::getUint16Ptr(tc);
	}

	PointerType* Type::getUint32Ptr(FTContext* tc)
	{
		return PointerType::getUint32Ptr(tc);
	}

	PointerType* Type::getUint64Ptr(FTContext* tc)
	{
		return PointerType::getUint64Ptr(tc);
	}

	PointerType* Type::getUint128Ptr(FTContext* tc)
	{
		return PointerType::getUint128Ptr(tc);
	}

	RangeType* Type::getRangeType(FTContext* tc)
	{
		return RangeType::get(tc);
	}

	CharType* Type::getCharType(FTContext* tc)
	{
		return CharType::get(tc);
	}

	StringType* Type::getStringType(FTContext* tc)
	{
		return StringType::get(tc);
	}

	AnyType* Type::getAnyType(FTContext* tc)
	{
		return AnyType::get(tc);
	}


















	std::string VoidType::str()
	{
		return "void";
	}

	std::string VoidType::encodedStr()
	{
		return "void";
	}

	bool VoidType::isTypeEqual(Type* other)
	{
		return other && other->isVoidType();
	}

	VoidType::VoidType()
	{
		// nothing
	}

	VoidType* VoidType::get(FTContext* tc)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		return tc->voidType;
	}






	std::string NullType::str()
	{
		return "nulltype";
	}

	std::string NullType::encodedStr()
	{
		return "nulltype";
	}

	bool NullType::isTypeEqual(Type* other)
	{
		return other && other->isNullType();
	}

	NullType::NullType()
	{
		// nothing
	}

	NullType* NullType::get(FTContext* tc)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		return tc->nullType;
	}
}


















