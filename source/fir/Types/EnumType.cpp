// EnumType.cpp
// Copyright (c) 2014 - 2016, zhiayang
// Licensed under the Apache License Version 2.0.


#include "ir/type.h"
#include "ir/value.h"
#include "ir/constant.h"


namespace fir
{
	EnumType::EnumType(const Identifier& name, Type* ct) : Type(TypeKind::Enum)
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
		if(other->kind != TypeKind::Enum)
			return false;

		return (this->typeName == other->toEnumType()->typeName) && (this->caseType->isTypeEqual(other->toEnumType()->caseType));
	}

	fir::ConstantValue* EnumType::getNameArray()
	{
		return this->runtimeNameArray;
	}

	fir::ConstantValue* EnumType::getCaseArray()
	{
		return this->runtimeCasesArray;
	}


	void EnumType::setNameArray(ConstantValue* arr)
	{
		this->runtimeNameArray = arr;
	}

	void EnumType::setCaseArray(ConstantValue* arr)
	{
		this->runtimeCasesArray = arr;
	}

	void EnumType::setCaseType(Type* t)
	{
		if(!this->caseType->isVoidType())
			error("cannot modify enum type! was previously '%s'", this->caseType);

		this->caseType = t;
	}




	static util::hash_map<Identifier, EnumType*> typeCache;

	EnumType* EnumType::get(const Identifier& name, Type* caseType)
	{
		if(auto it = typeCache.find(name); it != typeCache.end())
			error("enum with name '%s' already exists", name.str());

		else
			return (typeCache[name] = new EnumType(name, caseType));
	}

	fir::Type* EnumType::substitutePlaceholders(const util::hash_map<fir::Type*, fir::Type*>& subst)
	{
		if(this->containsPlaceholders())
			error("not supported!");

		return this;
	}
}












