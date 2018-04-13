// ArraySliceType.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ir/type.h"
#include "errors.h"

namespace fir
{
	ArraySliceType::ArraySliceType(Type* elmType, bool mut) : isSliceMutable(mut), arrayElementType(elmType)
	{
		iceAssert(this->arrayElementType);
	}

	Type* ArraySliceType::getElementType()
	{
		return this->arrayElementType;
	}

	bool ArraySliceType::isMutable()
	{
		return this->isSliceMutable;
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

		return this->arrayElementType->isTypeEqual(af->arrayElementType) && af->isSliceMutable == this->isSliceMutable;
	}

	ArraySliceType* ArraySliceType::get(Type* elementType, bool mut, FTContext* tc)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		// create.
		ArraySliceType* type = new ArraySliceType(elementType, mut);
		return dynamic_cast<ArraySliceType*>(tc->normaliseType(type));
	}

	ArraySliceType* ArraySliceType::getMutable(Type* elementType, FTContext* tc)
	{
		return get(elementType, true, tc);
	}

	ArraySliceType* ArraySliceType::getImmutable(Type* elementType, FTContext* tc)
	{
		return get(elementType, false, tc);
	}
}























