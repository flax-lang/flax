// StructType.cpp
// Copyright (c) 2014 - 2016, zhiayang
// Licensed under the Apache License Version 2.0.

#include "errors.h"
#include "ir/type.h"

#include "pts.h"

namespace fir
{
	// structs
	StructType::StructType(const Identifier& name, const std::vector<std::pair<std::string, Type*>>& mems, bool ispacked)
		: Type(TypeKind::Struct)
	{
		this->structName = name;
		this->isTypePacked = ispacked;

		this->setBody(mems);
	}

	static util::hash_map<Identifier, StructType*> typeCache;
	StructType* StructType::create(const Identifier& name, const std::vector<std::pair<std::string, Type*>>& members, bool packed)
	{
		if(auto it = typeCache.find(name); it != typeCache.end())
			error("struct with name '%s' already exists", name.str());

		else
			return (typeCache[name] = new StructType(name, members, packed));
	}

	StructType* StructType::createWithoutBody(const Identifier& name, bool isPacked)
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
	Identifier StructType::getTypeName()
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


	void StructType::setBody(const std::vector<std::pair<std::string, Type*>>& members)
	{
		size_t i = 0;
		for(auto p : members)
		{
			this->structMembers[p.first] = p.second;
			this->indexMap[p.first] = i;
			this->typeList.push_back(p.second);

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


















