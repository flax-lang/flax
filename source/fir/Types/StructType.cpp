// TupleType.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "errors.h"
#include "ir/type.h"

#include "pts.h"

namespace fir
{
	// structs
	StructType::StructType(const Identifier& name, const std::vector<std::pair<std::string, Type*>>& mems, bool ispacked)
	{
		this->structName = name;
		this->isTypePacked = ispacked;

		this->setBody(mems);
	}

	StructType* StructType::create(const Identifier& name, const std::vector<std::pair<std::string, Type*>>& members, FTContext* tc, bool packed)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		StructType* type = new StructType(name, members, packed);

		// special: need to check if new type has the same name
		for(auto t : tc->typeCache)
		{
			if(t->isStructType() && t->toStructType()->getTypeName() == name)
			{
				// check members.
				std::vector<Type*> tl1; for(auto p : members) tl1.push_back(p.second);
				std::vector<Type*> tl2; for(auto p : t->toStructType()->structMembers) tl2.push_back(p.second);

				if(!areTypeListsEqual(tl1, tl2))
					error("Conflicting types for named struct '%s':\n%s vs %s", name.str(), t, typeListToString(tl1));

				// ok.
				break;
			}
		}

		return dynamic_cast<StructType*>(tc->normaliseType(type));
	}

	StructType* StructType::createWithoutBody(const Identifier& name, FTContext* tc, bool isPacked)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		// special case: if no body, just return a type of the existing name.
		for(auto& t : tc->typeCache)
		{
			if(t->isStructType() && t->toStructType()->getTypeName() == name)
				return t->toStructType();
		}

		// if not, create a new one.
		return StructType::create(name, { }, tc, isPacked);
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
		StructType* os = dynamic_cast<StructType*>(other);
		if(!os) return false;
		if(this->structName != os->structName) return false;
		if(this->isTypePacked != os->isTypePacked) return false;

		// return areTypeListsEqual(this->typeList, os->typeList);
		return true;
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

	std::vector<Type*> StructType::getElements()
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
}


















