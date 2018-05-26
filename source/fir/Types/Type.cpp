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
	std::string Type::typeListToString(const std::initializer_list<Type*>& types, bool includeBraces)
	{
		return typeListToString(std::vector<Type*>(types.begin(), types.end()), includeBraces);
	}

	std::string Type::typeListToString(const std::vector<Type*>& types, bool braces)
	{
		// print types
		std::string str = (braces ? "{ " : "");
		for(auto t : types)
			str += t->str() + ", ";

		if(str.length() > 2)
			str = str.substr(0, str.length() - 2);

		return str + (braces ? " }" : "");
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

		PointerType* ptrthis = this->toPointerType();
		iceAssert(ptrthis);

		// ptrthis could only have been obtained by calling getPointerTo
		// on an already normalised type, so this should not be needed
		// newType = tc->normaliseType(newType);

		return ptrthis->baseType;
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
		if(this->kind != TypeKind::Primitive) error("not primitive type");
		return static_cast<PrimitiveType*>(this);
	}

	FunctionType* Type::toFunctionType()
	{
		if(this->kind != TypeKind::Function) error("not function type");
		return static_cast<FunctionType*>(this);
	}

	PointerType* Type::toPointerType()
	{
		if(this->kind != TypeKind::Pointer) error("not pointer type");
		return static_cast<PointerType*>(this);
	}

	StructType* Type::toStructType()
	{
		if(this->kind != TypeKind::Struct) error("not struct type");
		return static_cast<StructType*>(this);
	}

	ClassType* Type::toClassType()
	{
		if(this->kind != TypeKind::Class) error("not class type");
		return static_cast<ClassType*>(this);
	}

	TupleType* Type::toTupleType()
	{
		if(this->kind != TypeKind::Tuple) error("not tuple type");
		return static_cast<TupleType*>(this);
	}

	ArrayType* Type::toArrayType()
	{
		if(this->kind != TypeKind::Array) error("not array type");
		return static_cast<ArrayType*>(this);
	}

	DynamicArrayType* Type::toDynamicArrayType()
	{
		if(this->kind != TypeKind::DynamicArray) error("not dynamic array type");
		return static_cast<DynamicArrayType*>(this);
	}

	ArraySliceType* Type::toArraySliceType()
	{
		if(this->kind != TypeKind::ArraySlice) error("not array slice type");
		return static_cast<ArraySliceType*>(this);
	}

	RangeType* Type::toRangeType()
	{
		if(this->kind != TypeKind::Range) error("not range type");
		return static_cast<RangeType*>(this);
	}

	StringType* Type::toStringType()
	{
		if(this->kind != TypeKind::String) error("not string type");
		return static_cast<StringType*>(this);
	}

	EnumType* Type::toEnumType()
	{
		if(this->kind != TypeKind::Enum) error("not enum type");
		return static_cast<EnumType*>(this);
	}

	AnyType* Type::toAnyType()
	{
		if(this->kind != TypeKind::Any) error("not any type");
		return static_cast<AnyType*>(this);
	}

	NullType* Type::toNullType()
	{
		if(this->kind != TypeKind::Null) error("not null type");
		return static_cast<NullType*>(this);
	}

	ConstantNumberType* Type::toConstantNumberType()
	{
		if(this->kind != TypeKind::ConstantNumber) error("not constant number type");
		return static_cast<ConstantNumberType*>(this);
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
		iceAssert(this);
		return this->kind == TypeKind::ConstantNumber;
	}

	bool Type::isStructType()
	{
		iceAssert(this);
		return this->kind == TypeKind::Struct;
	}

	bool Type::isTupleType()
	{
		iceAssert(this);
		return this->kind == TypeKind::Tuple;
	}

	bool Type::isClassType()
	{
		iceAssert(this);
		return this->kind == TypeKind::Class;
	}

	bool Type::isPackedStruct()
	{
		iceAssert(this);
		return this->isStructType() && (this->toStructType()->isTypePacked);
	}

	bool Type::isArrayType()
	{
		iceAssert(this);
		return this->kind == TypeKind::Array;
	}

	bool Type::isFloatingPointType()
	{
		iceAssert(this);
		return this->kind == TypeKind::Primitive && (this->toPrimitiveType()->primKind == PrimitiveType::Kind::Floating);
	}

	bool Type::isIntegerType()
	{
		iceAssert(this);
		return this->kind == TypeKind::Primitive && (this->toPrimitiveType()->primKind == PrimitiveType::Kind::Integer);
	}

	bool Type::isSignedIntType()
	{
		iceAssert(this);
		return this->isIntegerType() && this->toPrimitiveType()->isSigned();
	}

	bool Type::isFunctionType()
	{
		iceAssert(this);
		return this->kind == TypeKind::Function;
	}

	bool Type::isPrimitiveType()
	{
		iceAssert(this);
		return this->kind == TypeKind::Primitive;
	}

	bool Type::isPointerType()
	{
		iceAssert(this);
		return this->kind == TypeKind::Pointer;
	}

	bool Type::isVoidType()
	{
		iceAssert(this);
		return this->kind == TypeKind::Void;
	}

	bool Type::isDynamicArrayType()
	{
		iceAssert(this);
		return this->kind == TypeKind::DynamicArray;
	}

	bool Type::isVariadicArrayType()
	{
		iceAssert(this);
		return this->isArraySliceType() && this->toArraySliceType()->isVariadicType();
	}

	bool Type::isArraySliceType()
	{
		iceAssert(this);
		return this->kind == TypeKind::ArraySlice;
	}

	bool Type::isRangeType()
	{
		iceAssert(this);
		return this->kind == TypeKind::Range;
	}

	bool Type::isStringType()
	{
		iceAssert(this);
		return this->kind == TypeKind::String;
	}

	bool Type::isCharType()
	{
		iceAssert(this);
		return this == fir::Type::getInt8();
	}

	bool Type::isEnumType()
	{
		iceAssert(this);
		return this->kind == TypeKind::Enum;
	}

	bool Type::isAnyType()
	{
		iceAssert(this);
		return this->kind == TypeKind::Any;
	}

	bool Type::isNullType()
	{
		iceAssert(this);
		return this->kind == TypeKind::Null;
	}

	bool Type::isBoolType()
	{
		iceAssert(this);
		return this == fir::Type::getBool();
	}

	bool Type::isMutablePointer()
	{
		iceAssert(this);
		return this->isPointerType() && this->toPointerType()->isMutable();
	}

	bool Type::isImmutablePointer()
	{
		iceAssert(this);
		return this->isPointerType() && !this->toPointerType()->isMutable();
	}

	bool Type::isCharSliceType()
	{
		iceAssert(this);
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












