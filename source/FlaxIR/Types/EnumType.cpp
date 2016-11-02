// EnumType.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "ir/type.h"
#include "ir/value.h"
#include "ir/constant.h"


namespace fir
{
	EnumType::EnumType(const Identifier& name, Type* ct, std::map<std::string, ConstantValue*> cs)
	{
		this->enumName = name;
		this->caseType = ct;

		for(auto c : cs)
		{
			if(c.second->getType() != this->caseType)
			{
				_error_and_exit("Mismatched types: case '%s' in enum '%s' has type '%s', in enum of type '%s'", c.first.c_str(),
					name.name.c_str(), c.second->getType()->str().c_str(), this->caseType->str().c_str());
			}
		}

		this->cases = cs;
	}

	Identifier EnumType::getEnumName()
	{
		return this->enumName;
	}

	ConstantValue* EnumType::getCaseWithName(std::string name)
	{
		if(this->cases.find(name) == this->cases.end())
			return 0;

		return this->cases[name];
	}

	ConstantArray* EnumType::getConstantArrayOfValues()
	{
		// well.
		std::vector<ConstantValue*> vals;
		for(auto p : this->cases)
			vals.push_back(p.second);

		return fir::ConstantArray::get(this->caseType, vals);
	}

	Type* EnumType::getCaseType()
	{
		return this->caseType;
	}

	std::string EnumType::str()
	{
		return "enum(" + this->enumName.name + ")";
	}

	std::string EnumType::encodedStr()
	{
		return this->enumName.str();
	}

	bool EnumType::isTypeEqual(Type* other)
	{
		EnumType* os = dynamic_cast<EnumType*>(other);
		if(!os) return false;
		if(this->enumName != os->enumName) return false;
		if(this->caseType != os->caseType) return false;
		if(this->cases != os->cases) return false;

		return true;
	}

	Type* EnumType::reify(std::map<std::string, Type*> names, FTContext* tc)
	{
		return this;
	}



	EnumType* EnumType::get(const Identifier& name, Type* caseType, std::map<std::string, ConstantValue*> _cases, FTContext* tc)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		EnumType* type = new EnumType(name, caseType, _cases);

		// special: need to check if new type has the same name
		for(auto t : tc->typeCache[0])
		{
			if(t->isEnumType() && t->toEnumType()->getEnumName() == name)
			{
				_error_and_exit("Enum '%s' already exists", name.str().c_str());
			}
		}

		return dynamic_cast<EnumType*>(tc->normaliseType(type));
	}
}












