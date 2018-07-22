// UnionType.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "errors.h"
#include "ir/type.h"

#include "pts.h"

namespace fir
{
	// structs
	UnionType::UnionType(const Identifier& name, const std::unordered_map<std::string, std::pair<size_t, Type*>>& mems)
		: Type(TypeKind::Union)
	{
		this->unionName = name;
		this->setBody(mems);
	}

	static std::unordered_map<Identifier, UnionType*> typeCache;
	UnionType* UnionType::create(const Identifier& name, const std::unordered_map<std::string, std::pair<size_t, Type*>>& mems)
	{
		if(auto it = typeCache.find(name); it != typeCache.end())
			error("Union with name '%s' already exists", name.str());

		else
			return (typeCache[name] = new UnionType(name, mems));
	}

	UnionType* UnionType::createWithoutBody(const Identifier& name)
	{
		return UnionType::create(name, { });
	}






	// various
	std::string UnionType::str()
	{
		return "union(" + this->unionName.name + ")";
	}

	std::string UnionType::encodedStr()
	{
		return this->unionName.str();
	}


	bool UnionType::isTypeEqual(Type* other)
	{
		if(other->kind != TypeKind::Union)
			return false;

		return (this->unionName == other->toUnionType()->unionName);
	}



	// struct stuff
	Identifier UnionType::getTypeName()
	{
		return this->unionName;
	}

	size_t UnionType::getVariantCount()
	{
		return this->variants.size();
	}


	std::unordered_map<std::string, Type*> UnionType::getVariants()
	{
		return this->variants;
	}

	bool UnionType::hasVariant(const std::string& name)
	{
		return this->variants.find(name) != this->variants.end();
	}

	Type* UnionType::getVariantType(size_t id)
	{
		if(auto it = this->revIndexMap.find(id); it != this->revIndexMap.end())
			return this->getVariantType(it->second);

		else
			error("no variant with id %d", id);
	}

	Type* UnionType::getVariantType(const std::string& name)
	{
		if(auto it = this->variants.find(name); it != this->variants.end())
			return it->second;

		else
			error("no variant named '%s' in union '%s'", name, this->getTypeName().str());
	}



	void UnionType::setBody(const std::unordered_map<std::string, std::pair<size_t, Type*>>& members)
	{
		for(const auto& [ n, p ] : members)
		{
			this->variants[n] = p.second;
			this->indexMap[n] = p.first;

			this->revIndexMap[p.first] = n;
		}
	}

	fir::Type* UnionType::substitutePlaceholders(const std::unordered_map<Type*, Type*>& subst)
	{
		if(this->containsPlaceholders())
			error("not supported!");

		return this;

		// std::vector<std::pair<std::string, Type*>> mems;
		// for(const auto& p : this->structMembers)
		// 	mems.push_back({ p.first, _substitute(subst, p.second) });

		// // return UnionType::get
		// auto it = typeCache.find(this->unionName);
		// iceAssert(it != typeCache.end());

		// auto old = it->second;
		// typeCache[this->structName] = new UnionType(this->structName, mems, this->isTypePacked);

		// delete old;
	}
}


















