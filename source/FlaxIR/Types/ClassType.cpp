// ClassType.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "errors.h"
#include "ir/type.h"
#include "ir/function.h"

namespace fir
{
	// structs
	ClassType::ClassType(Identifier name, std::deque<std::pair<std::string, Type*>> mems, std::deque<Function*> methods) : Type(FTypeKind::Class)
	{
		this->className = name;

		this->setMembers(mems);
		this->setMethods(methods);
	}


	ClassType* ClassType::create(Identifier name, std::deque<std::pair<std::string, Type*>> members, std::deque<Function*> methods, FTContext* tc)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		ClassType* type = new ClassType(name, members, methods);

		// special: need to check if new type has the same name
		for(auto t : tc->typeCache[0])
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

	ClassType* ClassType::createWithoutBody(Identifier name, FTContext* tc)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		// special case: if no body, just return a type of the existing name.
		for(auto t : tc->typeCache[0])
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
		if(this->typeList.size() == 0)
			return this->className.name/* + "<class>"*/;

		// auto s = typeListToString(this->typeList);
		return this->className.name/* + "<class: {" + s.substr(2, s.length() - 4) + "}>"*/;
	}

	std::string ClassType::encodedStr()
	{
		return this->className.str();
	}


	bool ClassType::isTypeEqual(Type* other)
	{
		ClassType* os = dynamic_cast<ClassType*>(other);
		if(!os) return false;
		if(this->typeKind != os->typeKind) return false;
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
		iceAssert(this->typeKind == FTypeKind::Class && "not class type");
		return this->typeList.size();
	}

	Type* ClassType::getElementN(size_t n)
	{
		iceAssert(this->typeKind == FTypeKind::Class && "not class type");
		iceAssert(n < this->typeList.size() && "out of bounds");

		return this->typeList[n];
	}

	Type* ClassType::getElement(std::string name)
	{
		iceAssert(this->typeKind == FTypeKind::Class && "not class type");
		iceAssert(this->classMembers.find(name) != this->classMembers.end() && "no such member");

		return this->classMembers[name];
	}

	size_t ClassType::getElementIndex(std::string name)
	{
		iceAssert(this->typeKind == FTypeKind::Class && "not class type");
		iceAssert(this->classMembers.find(name) != this->classMembers.end() && "no such member");

		return this->indexMap[name];
	}

	bool ClassType::hasElementWithName(std::string name)
	{
		return this->indexMap.find(name) != this->indexMap.end();
	}

	std::vector<Type*> ClassType::getElements()
	{
		iceAssert(this->typeKind == FTypeKind::Class && "not class type");
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
		iceAssert(this->typeKind == FTypeKind::Class && "not class type");

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
		iceAssert(this->typeKind == FTypeKind::Class && "not class type");

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
			if(mem.second->isParametricType())
			{
				if(reals.find(mem.second->toParametricType()->getName()) != reals.end())
				{
					auto t = reals[mem.second->toParametricType()->getName()];
					if(t->isParametricType())
					{
						error_and_exit("Cannot reify when the supposed real type of '%s' is still parametric",
							mem.second->toParametricType()->getName().c_str());
					}

					reifiedMems.push_back({ mem.first, t });
				}
				else
				{
					error_and_exit("Failed to reify, no type found for '%s'", mem.second->toParametricType()->getName().c_str());
				}
			}
			else
			{
				reifiedMems.push_back({ mem.first, mem.second });
			}
		}

		iceAssert(reifiedMems.size() == this->classMembers.size());


		// do the methods
		// uh... not yet.






		return ret;
	}
}












