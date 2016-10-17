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

	Type* StringType::reify(std::map<std::string, Type*> names, FTContext* tc)
	{
		error_and_exit("string type cannot be reified");
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



























	// char type

	std::string CharType::str()
	{
		return "char";
	}

	std::string CharType::encodedStr()
	{
		return "char";
	}

	bool CharType::isTypeEqual(Type* other)
	{
		// there's only ever one char type, so...
		return other && other->isCharType();
	}

	Type* CharType::reify(std::map<std::string, Type*> names, FTContext* tc)
	{
		error_and_exit("char type cannot be reified");
	}

	CharType::CharType()
	{
		// nothing
	}

	CharType* CharType::get(FTContext* tc)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		CharType* type = new CharType();
		return dynamic_cast<CharType*>(tc->normaliseType(type));
	}
}
















