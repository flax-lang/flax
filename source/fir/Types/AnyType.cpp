// AnyType.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ir/type.h"

namespace fir
{
	AnyType::AnyType()
	{
		// uh.
	}


	bool AnyType::isTypeEqual(Type* other)
	{
		return other && other->isAnyType();
	}

	AnyType* AnyType::get(FTContext* tc)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		AnyType* type = new AnyType();
		return dynamic_cast<AnyType*>(tc->normaliseType(type));
	}

	std::string AnyType::str()
	{
		return "any";
	}

	std::string AnyType::encodedStr()
	{
		return "any";
	}
}


















