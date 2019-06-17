// ClassType.cpp
// Copyright (c) 2014 - 2016, zhiayang
// Licensed under the Apache License Version 2.0.

#include "errors.h"
#include "ir/type.h"
#include "ir/function.h"

#include "pts.h"

namespace fir
{
	// structs
	ClassType::ClassType(const Identifier& name, const std::vector<std::pair<std::string, Type*>>& mems, const std::vector<Function*>& methods,
		const std::vector<Function*>& inits) : Type(TypeKind::Class)
	{
		this->className = name;

		this->setMembers(mems);
		this->setMethods(methods);
		this->setInitialiserFunctions(inits);
	}

	static util::hash_map<Identifier, ClassType*> typeCache;
	ClassType* ClassType::create(const Identifier& name, const std::vector<std::pair<std::string, Type*>>& members,
		const std::vector<Function*>& methods, const std::vector<Function*>& inits)
	{
		if(auto it = typeCache.find(name); it != typeCache.end())
			error("class with name '%s' already exists", name.str());

		else
			return (typeCache[name] = new ClassType(name, members, methods, inits));
	}

	ClassType* ClassType::createWithoutBody(const Identifier& name)
	{
		return ClassType::create(name, { }, { }, { });
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
		if(other->kind != TypeKind::Class)
			return false;

		return this->className == other->toClassType()->className;
	}



	// struct stuff
	Identifier ClassType::getTypeName()
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

	Type* ClassType::getElement(const std::string& name)
	{
		auto cls = this;
		while(cls->classMembers.find(name) == cls->classMembers.end())
			cls = cls->baseClass;

		iceAssert(cls && "no such member");
		return cls->classMembers[name];
	}



	size_t ClassType::getElementIndex(const std::string& name)
	{
		auto cls = this;
		while(cls->classMembers.find(name) == cls->classMembers.end())
			cls = cls->baseClass;

		iceAssert(cls && "no such member");

		// debuglog("index of %s = %d\n", name, cls->indexMap[name]);

		return cls->indexMap[name];
		// iceAssert(this->classMembers.find(name) != this->classMembers.end() && "no such member");

		// return this->indexMap[name];
	}

	void ClassType::setMembers(const std::vector<std::pair<std::string, Type*>>& members)
	{
		size_t i = 0;
		{
			auto cls = this->baseClass;
			while(cls)
			{
				i += cls->getElementCount();
				cls = cls->baseClass;
			}
		}


		for(auto p : members)
		{
			this->classMembers[p.first] = p.second;
			this->indexMap[p.first] = i;
			this->typeList.push_back(p.second);

			i++;
		}
	}

	bool ClassType::hasElementWithName(const std::string& name)
	{
		auto cls = this;
		while(cls && cls->classMembers.find(name) == cls->classMembers.end())
			cls = cls->baseClass;

		return cls != 0;


		// return this->indexMap.find(name) != this->indexMap.end();
	}



	std::vector<Type*> ClassType::getElements()
	{
		return this->typeList;
	}


	std::vector<Function*> ClassType::getInitialiserFunctions()
	{
		return this->initialiserList;
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

		error("no method with type '%s'", ftype);
	}


	bool ClassType::isInParentHierarchy(Type* base)
	{
		auto target = dcast(ClassType, base);
		if(!target) return false;

		auto cls = this;
		while(cls)
		{
			if(target == cls) return true;

			cls = cls->baseClass;
		}

		return false;
	}


	void ClassType::setMethods(const std::vector<Function*>& methods)
	{
		for(auto m : methods)
		{
			this->methodList.push_back(m);
			this->classMethodMap[m->getName().name].push_back(m);
		}
	}


	void ClassType::setInitialiserFunctions(const std::vector<Function*>& inits)
	{
		for(auto m : inits)
		{
			this->initialiserList.push_back(m);
			this->classMethodMap[m->getName().name].push_back(m);
		}
	}


	ClassType* ClassType::getBaseClass()
	{
		return this->baseClass;
	}

	void ClassType::setBaseClass(ClassType* ty)
	{
		this->baseClass = ty;

		//* keeps things simple.
		iceAssert(this->virtualMethodMap.empty() || !"cannot set base class after adding virtual methods");

		this->virtualMethodMap = this->baseClass->virtualMethodMap;
		this->virtualMethodCount = this->baseClass->virtualMethodCount;
		this->reverseVirtualMethodMap = this->baseClass->reverseVirtualMethodMap;
	}

	void ClassType::addVirtualMethod(Function* method)
	{
		//* what this does is compare the arguments without the first parameter,
		//* since that's going to be the self parameter, and that's going to be different
		auto withoutself = [](std::vector<Type*> p) -> std::vector<Type*> {
			p.erase(p.begin());

			return p;
		};

		auto matching = [&withoutself](const std::vector<Type*>& a, FunctionType* ft) -> bool {
			auto bp = withoutself(ft->getArgumentTypes());

			//* note: we don't call withoutself on 'a' because we expect that to already have been done
			//* before it was added.
			return Type::areTypeListsEqual(a, bp);
		};

		//* note: the 'reverse' virtual method map is to allow us, at translation time, to easily create the vtable without
		//* unnecessary searching. When we set a base class, we copy its 'reverse' map; thus, if we don't override anything,
		//* our vtable will just refer to the methods in the base class.

		//* but if we do override something, we just set the method in our 'reverse' map, which is what we'll use to build
		//* the vtable. simple?

		auto list = method->getType()->toFunctionType()->getArgumentTypes();

		// check every member of the current mapping -- not the fastest method i admit.
		bool found = false;
		for(auto vm : this->virtualMethodMap)
		{
			if(vm.first.first == method->getName().name && matching(vm.first.second, method->getType()->toFunctionType()))
			{
				found = true;
				this->virtualMethodMap[{ method->getName().name, withoutself(list) }] = vm.second;
				this->reverseVirtualMethodMap[vm.second] = method;
				break;
			}
		}

		if(!found)
		{
			// just make a new one.
			this->virtualMethodMap[{ method->getName().name, withoutself(list) }] = this->virtualMethodCount;
			this->reverseVirtualMethodMap[this->virtualMethodCount] = method;
			this->virtualMethodCount++;
		}
	}

	size_t ClassType::getVirtualMethodIndex(const std::string& name, FunctionType* ft)
	{
		auto withoutself = [](std::vector<Type*> p) -> std::vector<Type*> {
			p.erase(p.begin());
			return p;
		};

		auto list = ft->getArgumentTypes();

		if(auto it = this->virtualMethodMap.find({ name, withoutself(list) }); it != this->virtualMethodMap.end())
		{
			return it->second;
		}
		else
		{
			error("no method named '%s' matching signature '%s' in virtual method table of class '%s'",
				name, (Type*) ft, this->getTypeName().name);
		}
	}

	size_t ClassType::getVirtualMethodCount()
	{
		return this->virtualMethodCount;
	}


	Function* ClassType::getInlineInitialiser()
	{
		return this->inlineInitialiser;
	}

	void ClassType::setInlineInitialiser(Function* fn)
	{
		this->inlineInitialiser = fn;
	}

	fir::Type* ClassType::substitutePlaceholders(const util::hash_map<fir::Type*, fir::Type*>& subst)
	{
		if(this->containsPlaceholders())
			error("not supported!");

		return this;
	}
}













