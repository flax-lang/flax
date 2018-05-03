// DynamicArrayType.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ir/type.h"
#include "errors.h"

namespace fir
{
	DynamicArrayType::DynamicArrayType(Type* elmType) : Type(TypeKind::DynamicArray)
	{
		this->arrayElementType = elmType;
		iceAssert(this->arrayElementType);
	}

	Type* DynamicArrayType::getElementType()
	{
		return this->arrayElementType;
	}

	std::string DynamicArrayType::str()
	{
		return "[" + this->arrayElementType->str() + (this->isVariadic ? "...]" : "]");
	}

	std::string DynamicArrayType::encodedStr()
	{
		return "[" + this->arrayElementType->encodedStr() + (this->isVariadic ? "...]" : "]");
	}

	bool DynamicArrayType::isFunctionVariadic()
	{
		return this->isVariadic;
	}

	bool DynamicArrayType::isTypeEqual(Type* other)
	{
		if(other->kind != TypeKind::DynamicArray)
			return false;

		auto af = other->toDynamicArrayType();
		return this->arrayElementType->isTypeEqual(af->arrayElementType) && (this->isVariadic == af->isVariadic);
	}

	static TypeCache<DynamicArrayType> typeCache;
	DynamicArrayType* DynamicArrayType::get(Type* elementType)
	{
		return typeCache.getOrAddCachedType(new DynamicArrayType(elementType));
	}

	DynamicArrayType* DynamicArrayType::getVariadic(Type* elementType)
	{
		// create.
		DynamicArrayType* type = new DynamicArrayType(elementType);
		type->isVariadic = true;

		return typeCache.getOrAddCachedType(type);
	}

}























