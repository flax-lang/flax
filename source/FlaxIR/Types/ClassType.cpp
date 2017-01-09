// ClassType.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "errors.h"
#include "ir/type.h"
#include "ir/function.h"

namespace fir
{
	// structs
	ClassType::ClassType(const Identifier& name, std::deque<std::pair<std::string, Type*>> mems, std::deque<Function*> methods)
	{
		this->className = name;

		this->setMembers(mems);
		this->setMethods(methods);
	}


	ClassType* ClassType::create(const Identifier& name, std::deque<std::pair<std::string, Type*>> members,
		std::deque<Function*> methods, FTContext* tc)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		ClassType* type = new ClassType(name, members, methods);

		// special: need to check if new type has the same name
		for(auto t : tc->typeCache)
		{
			if(t->isClassType() && t->toClassType()->getClassName() == name)
			{
				// check members.
				std::deque<Type*> tl1; for(auto p : members) tl1.push_back(p.second);
				std::deque<Type*> tl2; for(auto p : t->toClassType()->classMembers) tl2.push_back(p.second);

				if(!areTypeListsEqual(tl1, tl2))
				{
					std::string mstr = typeListToString(tl1);
					error("Conflicting types for class %s:\n%s vs %s", name.str().c_str(), t->str().c_str(), mstr.c_str());
				}

				// ok.
				// early exit, since we should be checking this every time we add -- at most 1 with the same name at any moment.
				break;
			}
		}

		return dynamic_cast<ClassType*>(tc->normaliseType(type));
	}

	ClassType* ClassType::createWithoutBody(const Identifier& name, FTContext* tc)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		// special case: if no body, just return a type of the existing name.
		for(auto& t : tc->typeCache)
		{
			if(t->isClassType() && t->toClassType()->getClassName() == name)
				return t->toClassType();
		}

		// if not, create a new one.
		return ClassType::create(name, { }, { }, tc);
	}






	// various
	std::string ClassType::str()
	{
		return "class(" + this->className.name + ")";
	}

	std::string ClassType::encodedStr()
	{
		return this->className.str();
	}


	bool ClassType::isTypeEqual(Type* other)
	{
		ClassType* os = dynamic_cast<ClassType*>(other);
		if(!os) return false;
		if(this->className != os->className) return false;

		return true;
	}



	// struct stuff
	Identifier ClassType::getClassName()
	{
		return this->className;
	}

	size_t ClassType::getElementCount()
	{
		return this->typeList.size();
	}

	Type* ClassType::getElementN(size_t n)
	{
		iceAssert(n < this->typeList.size() && "out of bounds");

		return this->typeList[n];
	}

	Type* ClassType::getElement(std::string name)
	{
		iceAssert(this->classMembers.find(name) != this->classMembers.end() && "no such member");

		return this->classMembers[name];
	}

	size_t ClassType::getElementIndex(std::string name)
	{
		iceAssert(this->classMembers.find(name) != this->classMembers.end() && "no such member");

		return this->indexMap[name];
	}

	bool ClassType::hasElementWithName(std::string name)
	{
		return this->indexMap.find(name) != this->indexMap.end();
	}

	std::vector<Type*> ClassType::getElements()
	{
		return this->typeList;
	}



	std::vector<Function*> ClassType::getMethods()
	{
		return this->methodList;
	}

	std::vector<Function*> ClassType::getMethodsWithName(std::string id)
	{
		std::vector<Function*> ret;
		auto l = this->classMethodMap[id];

		for(auto f : l)
			ret.push_back(f);

		return ret;
	}

	Function* ClassType::getMethodWithType(FunctionType* ftype)
	{
		for(auto f : this->methodList)
		{
			if(f->getType() == ftype)
				return f;
		}

		error("no such function with type %s", ftype->str().c_str());
	}


	void ClassType::setMembers(std::deque<std::pair<std::string, Type*>> members)
	{
		size_t i = 0;
		for(auto p : members)
		{
			this->classMembers[p.first] = p.second;
			this->indexMap[p.first] = i;
			this->typeList.push_back(p.second);

			i++;
		}
	}

	void ClassType::setMethods(std::deque<Function*> methods)
	{
		for(auto m : methods)
		{
			this->methodList.push_back(m);
			this->classMethodMap[m->getName().name].push_back(m);
		}
	}





	ClassType* ClassType::reify(std::map<std::string, Type*> reals, FTContext* tc)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		ClassType* ret = ClassType::createWithoutBody(this->className);

		std::deque<std::pair<std::string, Type*>> reifiedMems;
		std::deque<Function*> reifiedMethods;

		for(auto mem : this->classMembers)
		{
			auto rfd = mem.second->reify(reals);
			if(rfd->isParametricType())
				_error_and_exit("Failed to reify, no type found for '%s'", mem.second->toParametricType()->getName().c_str());

			reifiedMems.push_back({ mem.first, rfd });
		}

		iceAssert(reifiedMems.size() == this->classMembers.size());


		// do the methods
		// uh... not yet.


		return ret;
	}
}













