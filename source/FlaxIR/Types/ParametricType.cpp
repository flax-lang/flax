// ParametricType.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "ir/type.h"

namespace fir
{
	ParametricType::ParametricType(std::string nm) : Type(FTypeKind::Parametric)
	{
		this->name = nm;
	}

	std::string ParametricType::getName()
	{
		return this->name;
	}

	std::string ParametricType::str()
	{
		return "$" + this->name;
	}

	std::string ParametricType::encodedStr()
	{
		return this->name;
	}

	bool ParametricType::isTypeEqual(Type* other)
	{
		if(!other->isParametricType()) return false;
		return this->name == other->toParametricType()->name;
	}




	ParametricType* ParametricType::get(std::string name, FTContext* tc)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		return tc->normaliseType(new ParametricType(name))->toParametricType();
	}



	ParametricType* ParametricType::reify(std::map<std::string, Type*> names, FTContext* tc)
	{
		error_and_exit("should not happen");
	}
}








