// RangeType.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ir/type.h"

namespace fir
{
	std::string RangeType::str()
	{
		return "range";
	}

	std::string RangeType::encodedStr()
	{
		return "range";
	}

	bool RangeType::isTypeEqual(Type* other)
	{
		// kappa.
		// there's only ever one string type, so...
		// lol.
		return other && other->isRangeType();
	}

	RangeType::RangeType()
	{
		// nothing
	}

	RangeType* RangeType::get(FTContext* tc)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		RangeType* type = new RangeType();
		return dynamic_cast<RangeType*>(tc->normaliseType(type));
	}
}



