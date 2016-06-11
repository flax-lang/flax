// LLVariableArrayType.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ir/type.h"

namespace fir
{
	LLVariableArrayType::LLVariableArrayType(Type* elmType) : Type(FTypeKind::LowLevelVariableArray)
	{
		this->arrayElementType = elmType;
	}

	Type* LLVariableArrayType::getElementType()
	{
		return this->arrayElementType;
	}

	std::string LLVariableArrayType::str()
	{
		return "[(variable) x " + this->arrayElementType->str() + "]";
	}

	std::string LLVariableArrayType::encodedStr()
	{
		return "[Vx" + this->arrayElementType->encodedStr() + "]";
	}


	bool LLVariableArrayType::isTypeEqual(Type* other)
	{
		LLVariableArrayType* af = dynamic_cast<LLVariableArrayType*>(other);
		if(!af) return false;

		return this->arrayElementType->isTypeEqual(af->arrayElementType);
	}

	LLVariableArrayType* LLVariableArrayType::get(Type* elementType, FTContext* tc)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		// create.
		LLVariableArrayType* type = new LLVariableArrayType(elementType);
		return dynamic_cast<LLVariableArrayType*>(tc->normaliseType(type));
	}
}











