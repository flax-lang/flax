// StructType.cpp
// Copyright (c) 2014 - 2016, zhiayang
// Licensed under the Apache License Version 2.0.

#include "errors.h"
#include "ir/type.h"

#include "pts.h"

namespace fir
{
	// structs
	StructType::StructType(const Name& name, const std::vector<std::pair<std::string, Type*>>& mems, bool ispacked)
		: Type(TypeKind::Struct), structName(name)
	{
		this->isTypePacked = ispacked;
		this->setBody(mems);
	}

	static util::hash_map<Name, StructType*> typeCache;
	StructType* StructType::create(const Name& name, const std::vector<std::pair<std::string, Type*>>& members, bool packed)
	{
		if(auto it = typeCache.find(name); it != typeCache.end())
			error("struct with name '%s' already exists", name.str());

		else
			return (typeCache[name] = new StructType(name, members, packed));
	}

	StructType* StructType::createWithoutBody(const Name& name, bool isPacked)
	{
		return StructType::create(name, { }, isPacked);
	}






	// various
	std::string StructType::str()
	{
		return "struct(" + this->structName.name + ")";
	}

	std::string StructType::encodedStr()
	{
		return this->structName.str();
	}


	bool StructType::isTypeEqual(Type* other)
	{
		if(other->kind != TypeKind::Struct)
			return false;

		return (this->structName == other->toStructType()->structName) && (this->isTypePacked == other->toStructType()->isTypePacked);
	}



	// struct stuff
	Name StructType::getTypeName()
	{
		return this->structName;
	}

	size_t StructType::getElementCount()
	{
		return this->typeList.size();
	}

	Type* StructType::getElementN(size_t n)
	{
		iceAssert(n < this->typeList.size() && "out of bounds");

		return this->typeList[n];
	}

	Type* StructType::getElement(const std::string& name)
	{
		iceAssert(this->structMembers.find(name) != this->structMembers.end() && "no such member");

		return this->structMembers[name];
	}

	size_t StructType::getElementIndex(const std::string& name)
	{
		iceAssert(this->structMembers.find(name) != this->structMembers.end() && "no such member");

		return this->indexMap[name];
	}

	bool StructType::hasElementWithName(const std::string& name)
	{
		return this->indexMap.find(name) != this->indexMap.end();
	}

	const std::vector<Type*>& StructType::getElements()
	{
		return this->typeList;
	}

	void StructType::addTraitImpl(TraitType* trt)
	{
		if(zfu::contains(this->implTraits, trt))
			error("'%s' already implements trait '%s'", this, trt);

		this->implTraits.push_back(trt);
	}

	bool StructType::implementsTrait(TraitType* trt)
	{
		return zfu::contains(this->implTraits, trt);
	}

	std::vector<TraitType*> StructType::getImplementedTraits()
	{
		return this->implTraits;
	}

	const util::hash_map<std::string, size_t>& StructType::getIndexMap()
	{
		return this->indexMap;
	}


	void StructType::setBody(const std::vector<std::pair<std::string, Type*>>& members)
	{
		size_t i = 0;
		for(const auto& [ name, ty ] : members)
		{
			this->structMembers[name] = ty;
			this->indexMap[name] = i;
			this->typeList.push_back(ty);

			i++;
		}
	}

	fir::Type* StructType::substitutePlaceholders(const util::hash_map<fir::Type*, fir::Type*>& subst)
	{
		if(this->containsPlaceholders())
			error("not supported!");

		return this;
	}
}


















