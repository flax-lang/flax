// UnionType.cpp
// Copyright (c) 2014 - 2016, zhiayang
// Licensed under the Apache License Version 2.0.

#include "errors.h"
#include "ir/type.h"

#include "pts.h"

namespace fir
{
	// structs
	UnionType::UnionType(const Name& name, const util::hash_map<std::string, std::pair<size_t, Type*>>& mems)
		: Type(TypeKind::Union), unionName(name)
	{
		this->setBody(mems);
	}

	static util::hash_map<Name, UnionType*> typeCache;
	UnionType* UnionType::create(const Name& name, const util::hash_map<std::string, std::pair<size_t, Type*>>& mems)
	{
		if(auto it = typeCache.find(name); it != typeCache.end())
			error("union with name '%s' already exists", name.str());

		else
			return (typeCache[name] = new UnionType(name, mems));
	}

	UnionType* UnionType::createWithoutBody(const Name& name)
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
	Name UnionType::getTypeName()
	{
		return this->unionName;
	}

	size_t UnionType::getVariantCount()
	{
		return this->variants.size();
	}

	size_t UnionType::getIdOfVariant(const std::string& name)
	{
		if(auto it = this->variants.find(name); it != this->variants.end())
			return it->second->variantId;

		else
			error("no variant with name '%s'", name);
	}

	const util::hash_map<std::string, UnionVariantType*>& UnionType::getVariants()
	{
		return this->variants;
	}

	bool UnionType::hasVariant(const std::string& name)
	{
		return this->variants.find(name) != this->variants.end();
	}

	UnionVariantType* UnionType::getVariant(size_t id)
	{
		if(auto it = this->indexMap.find(id); it != this->indexMap.end())
			return it->second;

		else
			error("no variant with id %d", id);
	}

	UnionVariantType* UnionType::getVariant(const std::string& name)
	{
		if(auto it = this->variants.find(name); it != this->variants.end())
			return it->second;

		else
			error("no variant named '%s' in union '%s'", name, this->getTypeName().str());
	}



	void UnionType::setBody(const util::hash_map<std::string, std::pair<size_t, Type*>>& members)
	{
		for(const auto& [ n, p ] : members)
		{
			auto uvt = new UnionVariantType(this, p.first, n, p.second);

			this->variants[n] = uvt;
			this->indexMap[p.first] = uvt;
		}
	}

	fir::Type* UnionType::substitutePlaceholders(const util::hash_map<Type*, Type*>& subst)
	{
		if(this->containsPlaceholders())
			error("not supported!");

		return this;
	}












	std::string UnionVariantType::str()
	{
		return this->parent->str() + "::" + this->name;
	}

	std::string UnionVariantType::encodedStr()
	{
		return this->parent->encodedStr() + "::" + this->name;
	}

	bool UnionVariantType::isTypeEqual(Type* other)
	{
		if(auto uvt = dcast(UnionVariantType, other))
			return uvt->parent == this->parent && uvt->name == this->name;

		return false;
	}

	fir::Type* UnionVariantType::substitutePlaceholders(const util::hash_map<fir::Type*, fir::Type*>& subst)
	{
		if(this->containsPlaceholders())
			error("not supported!");

		return this;
	}

	UnionVariantType::UnionVariantType(UnionType* p, size_t id, const std::string& name, Type* actual) : Type(TypeKind::UnionVariant)
	{
		this->parent = p;
		this->name = name;
		this->variantId = id;
		this->interiorType = actual;
	}
}


















