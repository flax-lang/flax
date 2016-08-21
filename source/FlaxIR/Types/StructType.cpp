// TupleType.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "errors.h"
#include "ir/type.h"

namespace fir
{
	// structs
	StructType::StructType(Identifier name, std::deque<std::pair<std::string, Type*>> mems, bool ispacked) : Type(FTypeKind::Struct)
	{
		this->structName = name;
		this->isTypePacked = ispacked;

		this->setBody(mems);
	}

	StructType* StructType::create(Identifier name, std::deque<std::pair<std::string, Type*>> members, FTContext* tc, bool packed)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		StructType* type = new StructType(name, members, packed);

		// special: need to check if new type has the same name
		for(auto t : tc->typeCache[0])
		{
			if(t->isStructType() && t->toStructType()->getStructName() == name)
			{
				// check members.
				std::deque<Type*> tl1; for(auto p : members) tl1.push_back(p.second);
				std::deque<Type*> tl2; for(auto p : t->toStructType()->structMembers) tl2.push_back(p.second);

				if(!areTypeListsEqual(tl1, tl2))
				{
					std::string mstr = typeListToString(tl1);
					error("Conflicting types for named struct %s:\n%s vs %s", name.str().c_str(), t->str().c_str(), mstr.c_str());
				}

				// ok.
				break;
			}
		}

		return dynamic_cast<StructType*>(tc->normaliseType(type));
	}

	StructType* StructType::createWithoutBody(Identifier name, FTContext* tc, bool isPacked)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		// special case: if no body, just return a type of the existing name.
		for(auto t : tc->typeCache[0])
		{
			if(t->isStructType() && t->toStructType()->getStructName() == name)
				return t->toStructType();
		}

		// if not, create a new one.
		return StructType::create(name, { }, tc, isPacked);
	}






	// various
	std::string StructType::str()
	{
		if(this->typeList.size() == 0)
			return this->structName.name/* + "<struct>"*/;

		// auto s = typeListToString(this->typeList);
		return this->structName.name/* + "<{" + s.substr(2, s.length() - 4) + "}>"*/;
	}

	std::string StructType::encodedStr()
	{
		return this->structName.str();
	}


	bool StructType::isTypeEqual(Type* other)
	{
		StructType* os = dynamic_cast<StructType*>(other);
		if(!os) return false;
		if(this->typeKind != os->typeKind) return false;
		if(this->isTypePacked != os->isTypePacked) return false;
		if(this->structName != os->structName) return false;

		return areTypeListsEqual(this->typeList, os->typeList);
	}



	// struct stuff
	Identifier StructType::getStructName()
	{
		return this->structName;
	}

	size_t StructType::getElementCount()
	{
		iceAssert(this->typeKind == FTypeKind::Struct && "not struct type");
		return this->typeList.size();
	}

	Type* StructType::getElementN(size_t n)
	{
		iceAssert(this->typeKind == FTypeKind::Struct && "not struct type");
		iceAssert(n < this->typeList.size() && "out of bounds");

		return this->typeList[n];
	}

	Type* StructType::getElement(std::string name)
	{
		iceAssert(this->typeKind == FTypeKind::Struct && "not struct type");
		iceAssert(this->structMembers.find(name) != this->structMembers.end() && "no such member");

		return this->structMembers[name];
	}

	size_t StructType::getElementIndex(std::string name)
	{
		iceAssert(this->typeKind == FTypeKind::Struct && "not struct type");
		iceAssert(this->structMembers.find(name) != this->structMembers.end() && "no such member");

		return this->indexMap[name];
	}

	bool StructType::hasElementWithName(std::string name)
	{
		return this->indexMap.find(name) != this->indexMap.end();
	}

	std::vector<Type*> StructType::getElements()
	{
		iceAssert(this->typeKind == FTypeKind::Struct && "not struct type");
		return this->typeList;
	}


	void StructType::setBody(std::deque<std::pair<std::string, Type*>> members)
	{
		iceAssert(this->typeKind == FTypeKind::Struct && "not struct type");

		size_t i = 0;
		for(auto p : members)
		{
			// if(this->structMembers.find(p.first) != this->structMembers.end()) iceAssert(0 && "duplicate member");

			this->structMembers[p.first] = p.second;
			this->indexMap[p.first] = i;
			this->typeList.push_back(p.second);

			i++;
		}
	}
}


















