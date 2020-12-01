// TraitType.cpp
// Copyright (c) 2019, zhiayang
// Licensed under the Apache License Version 2.0.


#include "errors.h"
#include "ir/type.h"

#include "pts.h"

namespace fir
{
	// structs
	TraitType::TraitType(const Name& name,  const std::vector<std::pair<std::string, FunctionType*>>& meths)
		: Type(TypeKind::Trait), traitName(name)
	{
		this->methods = meths;
	}

	static util::hash_map<Name, TraitType*> typeCache;
	TraitType* TraitType::create(const Name& name)
	{
		if(auto it = typeCache.find(name); it != typeCache.end())
			error("trait with name '%s' already exists", name.str());

		else
			return (typeCache[name] = new TraitType(name, { }));
	}





	// various
	std::string TraitType::str()
	{
		return "trait(" + this->traitName.name + ")";
	}

	std::string TraitType::encodedStr()
	{
		return this->traitName.str();
	}


	bool TraitType::isTypeEqual(Type* other)
	{
		if(other->kind != TypeKind::Struct)
			return false;

		return (this->traitName == other->toTraitType()->traitName);
	}



	// struct stuff
	Name TraitType::getTypeName()
	{
		return this->traitName;
	}

	size_t TraitType::getMethodCount()
	{
		return this->methods.size();
	}

	const std::vector<std::pair<std::string, FunctionType*>>& TraitType::getMethods()
	{
		return this->methods;
	}

	void TraitType::setMethods(const std::vector<std::pair<std::string, FunctionType*>>& meths)
	{
		this->methods = meths;
	}

	fir::Type* TraitType::substitutePlaceholders(const util::hash_map<fir::Type*, fir::Type*>& subst)
	{
		if(this->containsPlaceholders())
			error("not supported!");

		return this;
	}
}


















