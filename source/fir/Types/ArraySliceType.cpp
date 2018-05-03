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
		if(this->isCharSliceType())
			return (this->isSliceMutable ? "mut str" : "str");

		else
			return (this->isSliceMutable ? "[mut " : "[") + this->arrayElementType->str() + ":]";
	}

	std::string ArraySliceType::encodedStr()
	{
		if(this->isCharSliceType())
			return (this->isSliceMutable ? "mut str" : "str");

		else
			return (this->isSliceMutable ? "[mut " : "[") + this->arrayElementType->encodedStr() + ":]";
	}

	bool ArraySliceType::isTypeEqual(Type* other)
	{
		ArraySliceType* af = dynamic_cast<ArraySliceType*>(other);
		if(!af) return false;

		return this->arrayElementType->isTypeEqual(af->arrayElementType) && af->isSliceMutable == this->isSliceMutable;
	}

	static TypeCache<ArraySliceType> typeCache;
	ArraySliceType* ArraySliceType::get(Type* elementType, bool mut)
	{
		return typeCache.getOrAddCachedType(new ArraySliceType(elementType, mut));
	}

	ArraySliceType* ArraySliceType::getMutable(Type* elementType)
	{
		return get(elementType, true);
	}

	ArraySliceType* ArraySliceType::getImmutable(Type* elementType)
	{
		return get(elementType, false);
	}
}























