// Type.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ir/type.h"

namespace fir
{
	ArrayType::ArrayType(Type* elm, size_t num) : Type(FTypeKind::Array)
	{
		this->arrayElementType = elm;
		this->arraySize = num;
	}

	ArrayType* ArrayType::get(Type* elementType, size_t num, FTContext* tc)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		// create.
		ArrayType* type = new ArrayType(elementType, num);
		return dynamic_cast<ArrayType*>(tc->normaliseType(type));
	}

	// various
	std::string ArrayType::str()
	{
		return "[" + std::to_string(this->getArraySize()) + " x " + this->arrayElementType->str() + "]";
	}


	bool ArrayType::isTypeEqual(Type* other)
	{
		ArrayType* af = dynamic_cast<ArrayType*>(other);
		if(!af) return false;
		if(this->arraySize != af->arraySize) return false;

		return this->arrayElementType->isTypeEqual(af->arrayElementType);
	}






	// array stuff
	Type* ArrayType::getElementType()
	{
		iceAssert(this->typeKind == FTypeKind::Array && "not array type");
		return this->arrayElementType;
	}

	size_t ArrayType::getArraySize()
	{
		iceAssert(this->typeKind == FTypeKind::Array && "not array type");
		return this->arraySize;
	}
}


















