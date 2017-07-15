// ArraySliceType.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ir/type.h"
#include "errors.h"

namespace fir
{
	ArraySliceType::ArraySliceType(Type* elmType)
	{
		this->arrayElementType = elmType;
		iceAssert(this->arrayElementType);
	}

	Type* ArraySliceType::getElementType()
	{
		return this->arrayElementType;
	}

	std::string ArraySliceType::str()
	{
		return this->arrayElementType->str() + "[:]";
	}

	std::string ArraySliceType::encodedStr()
	{
		return this->arrayElementType->encodedStr() + "[:]";
	}

	bool ArraySliceType::isTypeEqual(Type* other)
	{
		ArraySliceType* af = dynamic_cast<ArraySliceType*>(other);
		if(!af) return false;

		return this->arrayElementType->isTypeEqual(af->arrayElementType);
	}

	ArraySliceType* ArraySliceType::get(Type* elementType, FTContext* tc)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		// create.
		ArraySliceType* type = new ArraySliceType(elementType);
		return dynamic_cast<ArraySliceType*>(tc->normaliseType(type));
	}


	ArraySliceType* ArraySliceType::reify(std::map<std::string, Type*> reals, FTContext* tc)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		return ArraySliceType::get(this->arrayElementType->reify(reals));
	}
}























