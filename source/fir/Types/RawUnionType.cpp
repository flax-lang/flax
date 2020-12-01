// RawUnionType.cpp
// Copyright (c) 2019, zhiayang
// Licensed under the Apache License Version 2.0.


#include "errors.h"
#include "ir/type.h"

#include "pts.h"

namespace fir
{
	// structs
	RawUnionType::RawUnionType(const Name& name, const util::hash_map<std::string, Type*>& mems)
		: Type(TypeKind::RawUnion), unionName(name)
	{
		this->setBody(mems);
	}

	static util::hash_map<Name, RawUnionType*> typeCache;
	RawUnionType* RawUnionType::create(const Name& name, const util::hash_map<std::string, Type*>& mems)
	{
		if(auto it = typeCache.find(name); it != typeCache.end())
			error("union with name '%s' already exists", name.str());

		else
			return (typeCache[name] = new RawUnionType(name, mems));
	}

	RawUnionType* RawUnionType::createWithoutBody(const Name& name)
	{
		return RawUnionType::create(name, { });
	}






	// various
	std::string RawUnionType::str()
	{
		return "raw_union(" + this->unionName.name + ")";
	}

	std::string RawUnionType::encodedStr()
	{
		return this->unionName.str();
	}


	bool RawUnionType::isTypeEqual(Type* other)
	{
		if(other->kind != TypeKind::Union)
			return false;

		return (this->unionName == other->toRawUnionType()->unionName);
	}



	// struct stuff
	Name RawUnionType::getTypeName()
	{
		return this->unionName;
	}

	size_t RawUnionType::getVariantCount()
	{
		return this->variants.size();
	}

	const util::hash_map<std::string, Type*>& RawUnionType::getVariants()
	{
		return this->variants;
	}

	bool RawUnionType::hasVariant(const std::string& name)
	{
		return this->variants.find(name) != this->variants.end();
	}

	Type* RawUnionType::getVariant(const std::string& name)
	{
		if(auto it = this->variants.find(name); it != this->variants.end())
			return it->second;

		else
			error("no variant named '%s' in union '%s'", name, this->getTypeName().str());
	}



	void RawUnionType::setBody(const util::hash_map<std::string, Type*>& members)
	{
		this->variants = members;
	}

	fir::Type* RawUnionType::substitutePlaceholders(const util::hash_map<Type*, Type*>& subst)
	{
		if(this->containsPlaceholders())
			error("not supported!");

		return this;
	}
}
