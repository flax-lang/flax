// DynamicArrayType.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ir/type.h"
#include "errors.h"

namespace fir
{
	DynamicArrayType::DynamicArrayType(Type* elmType)
	{
		this->arrayElementType = elmType;
		iceAssert(this->arrayElementType);
	}

	Type* DynamicArrayType::getElementType()
	{
		return this->arrayElementType;
	}

	std::string DynamicArrayType::str()
	{
		return "[? x " + this->arrayElementType->str() + "]";
	}

	std::string DynamicArrayType::encodedStr()
	{
		return "[Vx" + this->arrayElementType->encodedStr() + "]";
	}


	bool DynamicArrayType::isTypeEqual(Type* other)
	{
		DynamicArrayType* af = dynamic_cast<DynamicArrayType*>(other);
		if(!af) return false;

		return this->arrayElementType->isTypeEqual(af->arrayElementType);
	}

	DynamicArrayType* DynamicArrayType::get(Type* elementType, FTContext* tc)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		// create.
		DynamicArrayType* type = new DynamicArrayType(elementType);
		return dynamic_cast<DynamicArrayType*>(tc->normaliseType(type));
	}





	DynamicArrayType* DynamicArrayType::reify(std::map<std::string, Type*> reals, FTContext* tc)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		// basically return a new version of ourselves
		if(!this->arrayElementType->isParametricType())
			return this;

		ParametricType* tp = this->arrayElementType->toParametricType();

		if(reals.find(tp->getName()) != reals.end())
		{
			auto t = reals[tp->getName()];
			if(t->isParametricType())
				error_and_exit("Cannot reify when the supposed real type of '%s' is still parametric", tp->getName().c_str());

			return DynamicArrayType::get(t);
		}
		else
		{
			error_and_exit("Failed to reify, no type found for '%s'", tp->getName().c_str());
		}
	}
}























