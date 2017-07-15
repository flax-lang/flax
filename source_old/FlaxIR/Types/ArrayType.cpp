// Type.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ir/type.h"

namespace fir
{
	ArrayType::ArrayType(Type* elm, size_t num)
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
		return this->arrayElementType->str() + "[" + std::to_string(this->getArraySize()) + "]";
	}

	std::string ArrayType::encodedStr()
	{
		return this->arrayElementType->encodedStr() + "[" + std::to_string(this->getArraySize()) + "]";
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
		return this->arrayElementType;
	}

	size_t ArrayType::getArraySize()
	{
		return this->arraySize;
	}


	ArrayType* ArrayType::reify(std::map<std::string, Type*> reals, FTContext* tc)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		return ArrayType::get(this->arrayElementType->reify(reals), this->arraySize);
	}
}


















