// StringType.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ir/type.h"

namespace fir
{
	std::string StringType::str()
	{
		return "string";
	}

	std::string StringType::encodedStr()
	{
		return "string";
	}

	bool StringType::isTypeEqual(Type* other)
	{
		// kappa.
		// there's only ever one string type, so...
		// lol.
		return other && other->isStringType();
	}

	StringType::StringType()
	{
		// nothing
	}

	StringType* StringType::get(FTContext* tc)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		StringType* type = new StringType();
		return dynamic_cast<StringType*>(tc->normaliseType(type));
	}
}
















