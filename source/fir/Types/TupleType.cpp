// TupleType.cpp
// Copyright (c) 2014 - 2016, zhiayang
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

	const std::vector<Type*>& TupleType::getElements()
	{
		return this->members;
	}


	std::string TupleType::str()
	{
		return strprintf("(%s)", typeListToString(this->members));
	}

	std::string TupleType::encodedStr()
	{
		return strprintf("(%s)", typeListToString(this->members));
	}

	bool TupleType::isTypeEqual(Type* other)
	{
		if(other->kind != TypeKind::Tuple)
			return false;

		auto ot = other->toTupleType();
		return areTypeListsEqual(this->members, ot->members);
	}



	TupleType* TupleType::get(const std::vector<Type*>& mems)
	{
		return TypeCache::get().getOrAddCachedType(new TupleType(mems));
	}

	TupleType* TupleType::get(const std::initializer_list<Type*>& mems)
	{
		return TupleType::get(std::vector<Type*>(mems.begin(), mems.end()));
	}

	fir::Type* TupleType::substitutePlaceholders(const util::hash_map<fir::Type*, fir::Type*>& subst)
	{
		auto args = zfu::map(this->members, [&subst](auto t) -> auto { return t->substitutePlaceholders(subst); });

		return TupleType::get(members);
	}
}


















