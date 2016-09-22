// ParametricType.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "ir/type.h"

namespace fir
{
	ParametricType::ParametricType(std::string nm)
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



	Type* ParametricType::reify(std::map<std::string, Type*> names, FTContext* tc)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");


		if(names.find(this->name) == names.end())
			error_and_exit("Failed to reify, no type found for '%s'", this->name.c_str());


		return tc->normaliseType(names[this->name]);
	}
}








