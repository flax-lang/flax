// EnumType.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "ir/type.h"
#include "ir/value.h"
#include "ir/constant.h"


namespace fir
{
	EnumType::EnumType(const Identifier& name, Type* ct)
	{
		this->typeName = name;
		this->caseType = ct;
	}

	Identifier EnumType::getTypeName()
	{
		return this->typeName;
	}

	Type* EnumType::getCaseType()
	{
		return this->caseType;
	}

	std::string EnumType::str()
	{
		return "enum(" + this->typeName.name + ")";
	}

	std::string EnumType::encodedStr()
	{
		return this->typeName.str();
	}

	bool EnumType::isTypeEqual(Type* other)
	{
		EnumType* os = dynamic_cast<EnumType*>(other);
		if(!os) return false;
		if(this->typeName != os->typeName) return false;
		if(this->caseType != os->caseType) return false;

		return true;
	}


	fir::ConstantValue* EnumType::getNameArray()
	{
		return this->runtimeNameArray;
	}

	fir::ConstantValue* EnumType::getCaseArray()
	{
		return this->runtimeCasesArray;
	}


	void EnumType::setNameArray(fir::ConstantValue* arr)
	{
		this->runtimeNameArray = arr;
	}

	void EnumType::setCaseArray(fir::ConstantValue* arr)
	{
		this->runtimeCasesArray = arr;
	}




	EnumType* EnumType::get(const Identifier& name, Type* caseType, FTContext* tc)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		EnumType* type = new EnumType(name, caseType);

		// special: need to check if new type has the same name
		for(auto t : tc->typeCache)
		{
			if(t->isEnumType() && t->toEnumType()->getTypeName() == name)
				error("Enum '%s' already exists", name.str());
		}

		return dynamic_cast<EnumType*>(tc->normaliseType(type));
	}
}












