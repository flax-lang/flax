// DynamicArrayType.cpp
// Copyright (c) 2014 - 2016, zhiayang
// Licensed under the Apache License Version 2.0.

#include "ir/type.h"
#include "errors.h"

namespace fir
{
	DynamicArrayType::DynamicArrayType(Type* elmType) : Type(TypeKind::DynamicArray)
	{
		this->arrayElementType = elmType;
	}

	Type* DynamicArrayType::getElementType()
	{
		return this->arrayElementType;
	}

	std::string DynamicArrayType::str()
	{
		return strprintf("[%s]", this->arrayElementType->str());
	}

	std::string DynamicArrayType::encodedStr()
	{
		return strprintf("[%s]", this->arrayElementType->encodedStr());
	}

	bool DynamicArrayType::isTypeEqual(Type* other)
	{
		if(other->kind != TypeKind::DynamicArray)
			return false;

		return this->arrayElementType->isTypeEqual(other->toDynamicArrayType()->arrayElementType);
	}

	DynamicArrayType* DynamicArrayType::get(Type* elementType)
	{
		return TypeCache::get().getOrAddCachedType(new DynamicArrayType(elementType));
	}

	fir::Type* DynamicArrayType::substitutePlaceholders(const util::hash_map<fir::Type*, fir::Type*>& subst)
	{
		return DynamicArrayType::get(this->arrayElementType->substitutePlaceholders(subst));
	}
}























