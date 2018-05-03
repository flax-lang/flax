// TupleType.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "errors.h"
#include "ir/type.h"

namespace fir
{
	TupleType::TupleType(const std::vector<Type*>& mems) : Type(TypeKind::Tuple)
	{
		this->members = mems;
	}

	size_t TupleType::getElementCount()
	{
		return this->members.size();
	}

	Type* TupleType::getElementN(size_t n)
	{
		iceAssert(n < this->members.size());
		return this->members[n];
	}

	std::vector<Type*> TupleType::getElements()
	{
		return std::vector<Type*>(this->members.begin(), this->members.end());
	}


	std::string TupleType::str()
	{
		return typeListToString(this->members);
	}

	std::string TupleType::encodedStr()
	{
		return typeListToString(this->members);
	}

	bool TupleType::isTypeEqual(Type* other)
	{
		if(other->kind != TypeKind::Tuple)
			return false;

		auto ot = other->toTupleType();
		return areTypeListsEqual(this->members, ot->members);
	}



	static TypeCache<TupleType> typeCache;

	TupleType* TupleType::get(const std::vector<Type*>& mems)
	{
		return typeCache.getOrAddCachedType(new TupleType(mems));
	}

	TupleType* TupleType::get(const std::initializer_list<Type*>& mems)
	{
		return TupleType::get(std::vector<Type*>(mems.begin(), mems.end()));
	}
}


















