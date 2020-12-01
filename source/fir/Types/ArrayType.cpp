// Type.cpp
// Copyright (c) 2014 - 2016, zhiayang
// Licensed under the Apache License Version 2.0.

#include "ir/type.h"

namespace fir
{
	ArrayType::ArrayType(Type* elm, size_t num) : Type(TypeKind::Array)
	{
		this->arrayElementType = elm;
		this->arraySize = num;
	}

	ArrayType* ArrayType::get(Type* elementType, size_t num)
	{
		return TypeCache::get().getOrAddCachedType(new ArrayType(elementType, num));
	}

	std::string ArrayType::str()
	{
		return strprintf("[%s: %d]", this->arrayElementType->str(), this->getArraySize());
	}

	std::string ArrayType::encodedStr()
	{
		return strprintf("[%s: %d]", this->arrayElementType->encodedStr(), this->getArraySize());
	}



	bool ArrayType::isTypeEqual(Type* other)
	{
		if(other->kind != TypeKind::Array)
			return false;

		auto af = other->toArrayType();
		return this->arrayElementType->isTypeEqual(af->arrayElementType) && (this->arraySize == af->arraySize);
	}






	// array stuff
	Type* ArrayType::getElementType()
	{
		return this->arrayElementType;
	}

	size_t ArrayType::getArraySize()
	{
		return this->arraySize;
	}


	fir::Type* ArrayType::substitutePlaceholders(const util::hash_map<fir::Type*, fir::Type*>& subst)
	{
		return ArrayType::get(this->arrayElementType->substitutePlaceholders(subst), this->arraySize);
	}
}


















