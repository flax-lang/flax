// Type.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
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
		return strprintf("[%s: %ld]", this->arrayElementType->str(), this->getArraySize());
	}

	std::string ArrayType::encodedStr()
	{
		return strprintf("[%s: %ld]", this->arrayElementType->encodedStr(), this->getArraySize());
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
}


















