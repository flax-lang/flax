// SingleTypes.cpp
// Copyright (c) 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include "errors.h"
#include "ir/type.h"

namespace fir
{
	using PolySubst = util::hash_map<fir::Type*, fir::Type*>;

	static AnyType* singleAny = 0;
	AnyType::AnyType() : Type(TypeKind::Any)            { }
	std::string AnyType::str()                          { return "any"; }
	std::string AnyType::encodedStr()                   { return "any"; }
	bool AnyType::isTypeEqual(Type* other)              { return other && other->isAnyType(); }
	AnyType* AnyType::get()                             { return singleAny = (singleAny ? singleAny : new AnyType()); }
	fir::Type* AnyType::substitutePlaceholders(const PolySubst&)    { return this; }


	static BoolType* singleBool = 0;
	BoolType::BoolType() : Type(TypeKind::Bool)         { }
	std::string BoolType::str()                         { return "bool"; }
	std::string BoolType::encodedStr()                  { return "bool"; }
	bool BoolType::isTypeEqual(Type* other)             { return other && other->isBoolType(); }
	BoolType* BoolType::get()                           { return singleBool = (singleBool ? singleBool : new BoolType()); }
	fir::Type* BoolType::substitutePlaceholders(const PolySubst&)   { return this; }


	static VoidType* singleVoid = 0;
	VoidType::VoidType() : Type(TypeKind::Void)         { }
	std::string VoidType::str()                         { return "void"; }
	std::string VoidType::encodedStr()                  { return "void"; }
	bool VoidType::isTypeEqual(Type* other)             { return other && other->isVoidType(); }
	VoidType* VoidType::get()                           { return singleVoid = (singleVoid ? singleVoid : new VoidType()); }
	fir::Type* VoidType::substitutePlaceholders(const PolySubst&)   { return this; }


	static NullType* singleNull = 0;
	NullType::NullType() : Type(TypeKind::Null)         { }
	std::string NullType::str()                         { return "nulltype"; }
	std::string NullType::encodedStr()                  { return "nulltype"; }
	bool NullType::isTypeEqual(Type* other)             { return other && other->isNullType(); }
	NullType* NullType::get()                           { return singleNull = (singleNull ? singleNull : new NullType()); }
	fir::Type* NullType::substitutePlaceholders(const PolySubst&)   { return this; }


	static RangeType* singleRange = 0;
	RangeType::RangeType() : Type(TypeKind::Range)      { }
	std::string RangeType::str()                        { return "range"; }
	std::string RangeType::encodedStr()                 { return "range"; }
	bool RangeType::isTypeEqual(Type* other)            { return other && other->isRangeType(); }
	RangeType* RangeType::get()                         { return singleRange = (singleRange ? singleRange : new RangeType()); }
	fir::Type* RangeType::substitutePlaceholders(const PolySubst&)  { return this; }



	std::string PolyPlaceholderType::str()          { return strprintf("$%s/%d", this->name, this->group); }
	std::string PolyPlaceholderType::encodedStr()   { return strprintf("$%s/%d", this->name, this->group); }

	std::string PolyPlaceholderType::getName()      { return this->name; }
	int PolyPlaceholderType::getGroup()             { return this->group; }

	static std::vector<util::hash_map<std::string, PolyPlaceholderType*>> cache;
	PolyPlaceholderType* PolyPlaceholderType::get(const std::string& n, int group)
	{
		while(static_cast<size_t>(group) >= cache.size())
			cache.push_back({ });

		if(auto it = cache[group].find(n); it != cache[group].end())
			return it->second;

		return cache[group][n] = new PolyPlaceholderType(n, group);
	}

	bool PolyPlaceholderType::isTypeEqual(Type* other)
	{
		//! ACHTUNG !
		// performance optimisation: since all polys go through ::get, and we already guarantee interning
		// from that function, we should be able to just compare pointers.
		return (other == this);
	}

	PolyPlaceholderType::PolyPlaceholderType(const std::string& n, int g) : Type(TypeKind::PolyPlaceholder)
	{
		this->name = n;
		this->group = g;
	}


	static fir::Type* _substitute(const PolySubst& subst, fir::Type* t)
	{
		if(auto it = subst.find(t); it != subst.end())
			return it->second->substitutePlaceholders(subst);

		return t;
	}

	fir::Type* PolyPlaceholderType::substitutePlaceholders(const PolySubst& subst)
	{
		return _substitute(subst, this);
	}
}






















