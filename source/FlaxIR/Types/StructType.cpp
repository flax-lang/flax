// TupleType.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "errors.h"
#include "ir/type.h"

namespace fir
{
	// structs
	StructType::StructType(const Identifier& name, std::vector<std::pair<std::string, Type*>> mems, bool ispacked)
	{
		this->structName = name;
		this->isTypePacked = ispacked;

		this->setBody(mems);
	}

	StructType* StructType::create(const Identifier& name, std::vector<std::pair<std::string, Type*>> members, FTContext* tc, bool packed)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		StructType* type = new StructType(name, members, packed);

		// special: need to check if new type has the same name
		for(auto t : tc->typeCache)
		{
			if(t->isStructType() && t->toStructType()->getStructName() == name)
			{
				// check members.
				std::vector<Type*> tl1; for(auto p : members) tl1.push_back(p.second);
				std::vector<Type*> tl2; for(auto p : t->toStructType()->structMembers) tl2.push_back(p.second);

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

	StructType* StructType::createWithoutBody(const Identifier& name, FTContext* tc, bool isPacked)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		// special case: if no body, just return a type of the existing name.
		for(auto& t : tc->typeCache)
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
		if(this->isTypePacked != os->isTypePacked) return false;
		if(this->structName != os->structName) return false;

		// return areTypeListsEqual(this->typeList, os->typeList);
		return true;
	}



	// struct stuff
	Identifier StructType::getStructName()
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

	Type* StructType::getElement(std::string name)
	{
		iceAssert(this->structMembers.find(name) != this->structMembers.end() && "no such member");

		return this->structMembers[name];
	}

	size_t StructType::getElementIndex(std::string name)
	{
		iceAssert(this->structMembers.find(name) != this->structMembers.end() && "no such member");

		return this->indexMap[name];
	}

	bool StructType::hasElementWithName(std::string name)
	{
		return this->indexMap.find(name) != this->indexMap.end();
	}

	std::vector<Type*> StructType::getElements()
	{
		return this->typeList;
	}


	void StructType::setBody(std::vector<std::pair<std::string, Type*>> members)
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



	bool StructType::isGenericType()
	{
		return this->typeParameters.size() > 0;
	}

	std::vector<ParametricType*> StructType::getTypeParameters()
	{
		return this->typeParameters;
	}

	void StructType::addTypeParameter(ParametricType* t)
	{
		for(auto p : this->typeParameters)
		{
			if(p->getName() == t->getName())
				error("Type parameter '%s' already exists", p->getName().c_str());
		}

		this->typeParameters.push_back(t);
	}

	void StructType::addTypeParameters(std::vector<ParametricType*> ts)
	{
		for(auto t : ts)
			this->addTypeParameter(t);
	}

	bool StructType::isGenericInstantiation()
	{
		return this->isGenericInst;
	}

	void StructType::setGenericInstantiation()
	{
		this->isGenericInst = true;
	}



	StructType* StructType::reify(std::map<std::string, Type*> reals, FTContext* tc)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		if(this->isGenericType())
		{
			std::vector<std::pair<std::string, Type*>> reified;
			for(auto mem : this->structMembers)
			{
				auto rfd = mem.second->reify(reals);
				if(rfd->isParametricType())
				{
					_error_and_exit("Failed to reify type '%s', no type found for '%s'", this->str().c_str(),
						mem.second->toParametricType()->getName().c_str());
				}

				reified.push_back({ mem.first, rfd });
			}

			iceAssert(reified.size() == this->structMembers.size());

			auto ret = StructType::create(Identifier(this->structName.str() + fir::mangleGenericTypes(reals), IdKind::Struct), reified);
			ret->setGenericInstantiation();

			return ret;
		}
		else
		{
			return this;
		}
	}
}


















