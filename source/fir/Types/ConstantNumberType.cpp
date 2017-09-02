// ConstantNumberType.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ir/type.h"

namespace fir
{
	std::string ConstantNumberType::str()
	{
		return "number";
	}

	std::string ConstantNumberType::encodedStr()
	{
		return "number";
	}

	bool ConstantNumberType::isTypeEqual(Type* other)
	{
		return other && other->isConstantNumberType();
	}

	ConstantNumberType::ConstantNumberType(mpfr::mpreal n)
	{
		// nothing
		this->number = n;
	}

	ConstantNumberType* ConstantNumberType::get(mpfr::mpreal n, FTContext* tc)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		return new ConstantNumberType(n);
	}

	mpfr::mpreal ConstantNumberType::getValue()
	{
		return this->number;
	}
}



