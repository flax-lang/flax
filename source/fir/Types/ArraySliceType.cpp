// ArraySliceType.cpp
// Copyright (c) 2014 - 2016, zhiayang
// Licensed under the Apache License Version 2.0.

#include "ir/type.h"
#include "errors.h"

namespace fir
{
	ArraySliceType::ArraySliceType(Type* elmType, bool mut) : Type(TypeKind::ArraySlice)
	{
		this->isSliceMutable = mut;
		this->arrayElementType = elmType;

		this->isVariadic = false;
	}

	Type* ArraySliceType::getElementType()
	{
		return this->arrayElementType;
	}

	bool ArraySliceType::isMutable()
	{
		return this->isSliceMutable;
	}

	Type* ArraySliceType::getDataPointerType()
	{
		if(this->isSliceMutable)    return this->arrayElementType->getMutablePointerTo();
		else                        return this->arrayElementType->getPointerTo();
	}

	std::string ArraySliceType::str()
	{
		if(this->isCharSliceType())
			return (this->isSliceMutable ? "mut str" : "str");

		else
			return strprintf("[%s%s:%s]", this->isSliceMutable ? "mut " : "", this->arrayElementType->str(), this->isVariadic ? " ..." : "");
	}

	std::string ArraySliceType::encodedStr()
	{
		if(this->isCharSliceType())
			return (this->isSliceMutable ? "mut str" : "str");

		else
			return strprintf("[%s%s:%s]", this->isSliceMutable ? "mut " : "", this->arrayElementType->encodedStr(), this->isVariadic ? " ..." : "");
	}

	bool ArraySliceType::isTypeEqual(Type* other)
	{
		if(other->kind != TypeKind::ArraySlice)
			return false;

		auto af = other->toArraySliceType();
		return this->arrayElementType->isTypeEqual(af->arrayElementType) && (this->isSliceMutable == af->isSliceMutable)
			&& (this->isVariadic == af->isVariadic);
	}

	bool ArraySliceType::isVariadicType()
	{
		return this->isVariadic;
	}

	ArraySliceType* ArraySliceType::get(Type* elementType, bool mut)
	{
		return TypeCache::get().getOrAddCachedType(new ArraySliceType(elementType, mut));
	}

	ArraySliceType* ArraySliceType::getMutable(Type* elementType)
	{
		return get(elementType, true);
	}

	ArraySliceType* ArraySliceType::getImmutable(Type* elementType)
	{
		return get(elementType, false);
	}

	ArraySliceType* ArraySliceType::getVariadic(Type* elementType)
	{
		auto a = new ArraySliceType(elementType, false);
		a->isVariadic = true;

		return TypeCache::get().getOrAddCachedType(a);
	}


	fir::Type* ArraySliceType::substitutePlaceholders(const util::hash_map<fir::Type*, fir::Type*>& subst)
	{
		return ArraySliceType::get(this->arrayElementType->substitutePlaceholders(subst), this->isSliceMutable);
	}
}























