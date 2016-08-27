// TupleType.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
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





	TupleType* TupleType::reify(std::map<std::string, Type*> reals, FTContext* tc)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		std::vector<Type*> reified;
		for(auto mem : this->members)
		{
			if(mem->isParametricType())
			{
				if(reals.find(mem->toParametricType()->getName()) != reals.end())
				{
					auto t = reals[mem->toParametricType()->getName()];
					if(t->isParametricType())
					{
						error_and_exit("Cannot reify when the supposed real type of '%s' is still parametric",
							mem->toParametricType()->getName().c_str());
					}

					reified.push_back(t);
				}
				else
				{
					error_and_exit("Failed to reify, no type found for '%s'", mem->toParametricType()->getName().c_str());
				}
			}
			else
			{
				reified.push_back(mem);
			}
		}

		iceAssert(reified.size() == this->members.size());
		return TupleType::get(reified);
	}
}


















