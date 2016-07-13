// Type.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ir/type.h"

namespace fir
{
	// structs
	StructType::StructType(std::string name, std::deque<Type*> mems, bool islit, bool ispacked)
		: Type(islit ? FTypeKind::LiteralStruct : FTypeKind::NamedStruct)
	{
		this->structName = name;
		this->structMembers = mems;
		this->isTypePacked = ispacked;
	}

	StructType* StructType::createNamed(std::string name, std::deque<Type*> members, FTContext* tc, bool packed)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		StructType* type = new StructType(name, members, false, packed);

		// special: need to check if new type has the same name
		for(auto t : tc->typeCache[0])
		{
			if(t->isStructType() && t->toStructType()->isNamedStruct() && t->toStructType()->getStructName() == name)
			{
				// check members.
				if(!areTypeListsEqual(members, t->toStructType()->structMembers))
				{
					std::string mstr = typeListToString(members);
					error("Conflicting types for named struct %s:\n%s vs %s", name.c_str(), t->str().c_str(), mstr.c_str());
				}

				// ok.
				break;
			}
		}

		return dynamic_cast<StructType*>(tc->normaliseType(type));
	}

	StructType* StructType::createNamedWithoutBody(std::string name, FTContext* tc, bool isPacked)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		// special case: if no body, just return a type of the existing name.
		for(auto t : tc->typeCache[0])
		{
			if(t->isStructType() && t->toStructType()->isNamedStruct() && t->toStructType()->getStructName() == name)
				return t->toStructType();
		}

		// if not, create a new one.
		return createNamed(name, { }, tc, isPacked);
	}

	StructType* StructType::createNamed(std::string name, std::vector<Type*> members, FTContext* tc, bool packed)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		std::deque<Type*> dmems;
		for(auto m : members)
			dmems.push_back(m);

		return StructType::createNamed(name, dmems, tc, packed);
	}

	StructType* StructType::createNamed(std::string name, std::initializer_list<Type*> members, FTContext* tc, bool packed)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		std::deque<Type*> dmems = members;
		return StructType::createNamed(name, dmems, tc, packed);
	}



	StructType* StructType::getLiteral(std::deque<Type*> members, FTContext* tc, bool packed)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		iceAssert(members.size() > 0 && "literal struct must have body at init");

		StructType* type = new StructType("__LITERAL_STRUCT__", members, true, packed);
		return dynamic_cast<StructType*>(tc->normaliseType(type));
	}

	StructType* StructType::getLiteral(std::vector<Type*> members, FTContext* tc, bool packed)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		iceAssert(members.size() > 0 && "literal struct must have body at init");

		std::deque<Type*> dmems;
		for(auto m : members)
			dmems.push_back(m);

		return StructType::getLiteral(dmems, tc, packed);
	}

	StructType* StructType::getLiteral(std::initializer_list<Type*> members, FTContext* tc, bool packed)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		iceAssert(members.size() > 0 && "literal struct must have body at init");

		std::deque<Type*> dmems = members;
		return StructType::getLiteral(dmems, tc, packed);
	}






	// various
	std::string StructType::str()
	{
		if(this->isNamedStruct())
		{
			if(this->structMembers.size() == 0)
				return this->structName + "<???>";

			auto s = typeListToString(this->structMembers);
			return this->structName + "<{" + s.substr(2, s.length() - 4) + "}>";
		}
		else if(this->isLiteralStruct())
		{
			return typeListToString(this->structMembers);
		}
		else
		{
			iceAssert(0);
		}
	}

	std::string StructType::encodedStr()
	{
		if(this->isNamedStruct())
		{
			return this->structName;
		}
		else if(this->isLiteralStruct())
		{
			return typeListToString(this->structMembers);
		}
		else
		{
			iceAssert(0);
		}
	}


	bool StructType::isTypeEqual(Type* other)
	{
		StructType* os = dynamic_cast<StructType*>(other);
		if(!os) return false;
		if(this->typeKind != os->typeKind) return false;
		if(this->isTypePacked != os->isTypePacked) return false;
		if(this->structMembers.size() != os->structMembers.size()) return false;
		if(this->isLiteralStruct() != os->isLiteralStruct()) return false;
		if(this->baseType && (!this->baseType->isTypeEqual(os->baseType))) return false;

		// compare names
		if(!this->isLiteralStruct() && (this->structName != os->structName)) return false;

		for(size_t i = 0; i < this->structMembers.size(); i++)
		{
			if(!this->structMembers[i]->isTypeEqual(os->structMembers[i]))
				return false;
		}

		return true;
	}



	// struct stuff
	std::string StructType::getStructName()
	{
		iceAssert(this->typeKind == FTypeKind::NamedStruct && "not named struct");
		return this->structName;
	}

	size_t StructType::getElementCount()
	{
		iceAssert(this->typeKind == FTypeKind::NamedStruct || this->typeKind == FTypeKind::LiteralStruct && "not struct type");
		return this->structMembers.size();
	}

	Type* StructType::getElementN(size_t n)
	{
		iceAssert(this->typeKind == FTypeKind::NamedStruct || this->typeKind == FTypeKind::LiteralStruct && "not struct type");
		iceAssert(n < this->structMembers.size() && "out of bounds");

		return this->structMembers[n];
	}

	std::vector<Type*> StructType::getElements()
	{
		iceAssert(this->typeKind == FTypeKind::NamedStruct || this->typeKind == FTypeKind::LiteralStruct && "not struct type");

		std::vector<Type*> vmems;
		for(auto m : this->structMembers)
			vmems.push_back(m);

		return vmems;
	}


	void StructType::setBody(std::initializer_list<Type*> members)
	{
		iceAssert(this->typeKind == FTypeKind::NamedStruct || this->typeKind == FTypeKind::LiteralStruct && "not struct type");

		this->structMembers = members;
	}

	void StructType::setBody(std::vector<Type*> members)
	{
		iceAssert(this->typeKind == FTypeKind::NamedStruct || this->typeKind == FTypeKind::LiteralStruct && "not struct type");

		std::deque<Type*> dmems;
		for(auto m : members)
			dmems.push_back(m);

		this->structMembers = dmems;
	}

	void StructType::setBody(std::deque<Type*> members)
	{
		iceAssert(this->typeKind == FTypeKind::NamedStruct || this->typeKind == FTypeKind::LiteralStruct && "not struct type");

		this->structMembers = members;
	}



	void StructType::setBaseType(StructType* base)
	{
		this->baseType = base;
	}

	void StructType::clearBaseType()
	{
		this->baseType = 0;
	}

	StructType* StructType::getBaseType()
	{
		return this->baseType;
	}

	bool StructType::isABaseTypeOf(Type* ot)
	{
		if(!ot) return false;

		StructType* ost = ot->toStructType();
		if(!ost) return false;

		StructType* base = ost->getBaseType();
		while(base != 0)
		{
			if(base->isTypeEqual(this))
				return true;

			base = base->getBaseType();
		}

		return false;
	}

	bool StructType::isADerivedTypeOf(Type* ot)
	{
		if(!ot) return false;

		StructType* ost = ot->toStructType();
		if(!ost) return false;

		StructType* base = this->getBaseType();
		while(base != 0)
		{
			if(base->isTypeEqual(this))
				return true;

			base = base->getBaseType();
		}

		return false;
	}






	void StructType::deleteType(FTContext* tc)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		if(this->typeKind != FTypeKind::NamedStruct) return;

		for(auto it = tc->typeCache[0].begin(); it != tc->typeCache[0].end(); it++)
		{
			if(this->isTypeEqual(*it))
			{
				tc->typeCache[0].erase(it);
				break;
			}
		}

		// todo: safe?
		delete this;
	}
}


















