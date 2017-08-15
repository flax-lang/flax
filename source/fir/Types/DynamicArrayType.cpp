// DynamicArrayType.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ir/type.h"
#include "errors.h"

namespace fir
{
	DynamicArrayType::DynamicArrayType(Type* elmType)
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
		return this->arrayElementType->str() + (this->isVariadic ? "[...]" : "[]");
	}

	std::string DynamicArrayType::encodedStr()
	{
		return this->arrayElementType->encodedStr() + (this->isVariadic ? "[...]" : "[]");
	}

	bool DynamicArrayType::isFunctionVariadic()
	{
		return this->isVariadic;
	}

	bool DynamicArrayType::isTypeEqual(Type* other)
	{
		DynamicArrayType* af = dynamic_cast<DynamicArrayType*>(other);
		if(!af) return false;
		if(af->isVariadic != this->isVariadic) return false;

		return this->arrayElementType->isTypeEqual(af->arrayElementType);
	}

	DynamicArrayType* DynamicArrayType::get(Type* elementType, FTContext* tc)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		// create.
		DynamicArrayType* type = new DynamicArrayType(elementType);
		return dynamic_cast<DynamicArrayType*>(tc->normaliseType(type));
	}

	DynamicArrayType* DynamicArrayType::getVariadic(Type* elementType, FTContext* tc)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		// create.
		DynamicArrayType* type = new DynamicArrayType(elementType);
		type->isVariadic = true;

		return dynamic_cast<DynamicArrayType*>(tc->normaliseType(type));
	}

}























