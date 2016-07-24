// TupleType.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "errors.h"
#include "ir/type.h"

namespace fir
{
	TupleType::TupleType(std::vector<Type*> mems) : Type(FTypeKind::Tuple), members(mems)
	{
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
		TupleType* ot = dynamic_cast<TupleType*>(other);
		if(!ot) return false;

		return areTypeListsEqual(this->members, ot->members);
	}



	TupleType* TupleType::get(std::initializer_list<Type*> mems, FTContext* tc)
	{
		return TupleType::get(std::vector<Type*>(mems.begin(), mems.end()), tc);
	}

	TupleType* TupleType::get(std::deque<Type*> mems, FTContext* tc)
	{
		return TupleType::get(std::vector<Type*>(mems.begin(), mems.end()), tc);
	}

	TupleType* TupleType::get(std::vector<Type*> mems, FTContext* tc)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		iceAssert(mems.size() > 0 && "empty tuple is not supported");

		TupleType* type = new TupleType(mems);
		return dynamic_cast<TupleType*>(tc->normaliseType(type));
	}
}


















