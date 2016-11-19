// ParameterPackType.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ir/type.h"
#include "errors.h"

namespace fir
{
	ParameterPackType::ParameterPackType(Type* elmType)
	{
		this->arrayElementType = elmType;
		iceAssert(this->arrayElementType);
	}

	Type* ParameterPackType::getElementType()
	{
		return this->arrayElementType;
	}

	std::string ParameterPackType::str()
	{
		return this->arrayElementType->str() + "[...]";
	}

	std::string ParameterPackType::encodedStr()
	{
		return this->arrayElementType->encodedStr() + "[V]";
	}


	bool ParameterPackType::isTypeEqual(Type* other)
	{
		ParameterPackType* af = dynamic_cast<ParameterPackType*>(other);
		if(!af) return false;

		return this->arrayElementType->isTypeEqual(af->arrayElementType);
	}

	ParameterPackType* ParameterPackType::get(Type* elementType, FTContext* tc)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		// create.
		ParameterPackType* type = new ParameterPackType(elementType);
		return dynamic_cast<ParameterPackType*>(tc->normaliseType(type));
	}





	ParameterPackType* ParameterPackType::reify(std::map<std::string, Type*> reals, FTContext* tc)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		return ParameterPackType::get(this->arrayElementType->reify(reals));
	}
}





























