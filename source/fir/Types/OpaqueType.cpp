// OpaqueType.cpp
// Copyright (c) 2019, zhiayang
// Licensed under the Apache License Version 2.0.

#include "ir/type.h"
#include "ir/value.h"
#include "ir/constant.h"


namespace fir
{
	OpaqueType::OpaqueType(const std::string& name, size_t size) : Type(TypeKind::Opaque)
	{
		this->typeName = name;
		this->typeSizeInBits = size;
	}

	std::string OpaqueType::str()
	{
		return strprintf("opaque(%s)", this->typeName);
	}

	std::string OpaqueType::encodedStr()
	{
		return strprintf("opaque(%s)", this->typeName);
	}

	bool OpaqueType::isTypeEqual(Type* other)
	{
		return other && other->isOpaqueType() && other->toOpaqueType()->typeName == this->typeName;
	}

	fir::Type* OpaqueType::substitutePlaceholders(const util::hash_map<fir::Type*, fir::Type*>& subst)
	{
		return this;
	}


	static util::hash_map<std::string, OpaqueType*> typeCache;
	OpaqueType* OpaqueType::get(const std::string& name, size_t size)
	{
		if(size < 8)
			error("types must be >= 8 bits (for now) (%s)", name);


		if(auto it = typeCache.find(name); it != typeCache.end())
			return it->second;

		else
			return (typeCache[name] = new OpaqueType(name, size));
	}
}
