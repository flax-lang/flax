// ClassType.cpp
// Copyright (c) 2014 - 2016, zhiayang
// Licensed under the Apache License Version 2.0.

#include "errors.h"
#include "ir/type.h"
#include "ir/function.h"

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

	Type* ClassType::getElement(const std::string& name)
	{
		auto cls = this;
		while(cls->classMembers.find(name) == cls->classMembers.end())
			cls = cls->baseClass;

		iceAssert(cls && "no such member");
		return cls->classMembers[name];
	}

	size_t ClassType::getAbsoluteElementIndex(const std::string& name)
	{
		auto cls = this;
		while(cls->classMembers.find(name) == cls->classMembers.end())
			cls = cls->baseClass;

		iceAssert(cls && "no such member");

		return cls->indexMap[name];
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

		for(const auto& [ name, ty ] : members)
		{
			this->classMembers[name] = ty;
			this->indexMap[name] = i;

			this->nameList.push_back(name);
			this->typeList.push_back(ty);

			i++;
		}
	}

	bool ClassType::hasElementWithName(const std::string& name)
	{
		auto cls = this;
		while(cls && cls->classMembers.find(name) == cls->classMembers.end())
			cls = cls->baseClass;

		return cls != 0;
	}



	const std::vector<Type*>& ClassType::getElements()
	{
		return this->typeList;
	}

	const std::vector<std::string>& ClassType::getNameList()
	{
		return this->nameList;
	}

	std::vector<Type*> ClassType::getAllElementsIncludingBase()
	{
		std::vector<Type*> ret;

		std::function<void (ClassType*, std::vector<Type*>*)>
		addMembers = [&addMembers](ClassType* cls, std::vector<Type*>* mems) -> void {

			if(!cls) return;

			addMembers(cls->getBaseClass(), mems);

			for(auto f : cls->getElements())
				mems->push_back(f);
		};

		addMembers(this, &ret);

		return ret;
	}

	const util::hash_map<std::string, size_t>& ClassType::getElementNameMap()
	{
		return this->indexMap;
	}








	const std::vector<Function*>& ClassType::getInitialiserFunctions()
	{
		return this->initialiserList;
	}

	void ClassType::setDestructor(Function* f)
	{
		this->destructor = f;
	}

	void ClassType::setCopyConstructor(Function* f)
	{
		this->copyConstructor = f;
	}

	void ClassType::setMoveConstructor(Function* f)
	{
		this->moveConstructor = f;
	}


	Function* ClassType::getDestructor()
	{
		return this->destructor;
	}

	Function* ClassType::getCopyConstructor()
	{
		return this->copyConstructor;
	}

	Function* ClassType::getMoveConstructor()
	{
		return this->moveConstructor;
	}




	const std::vector<Function*>& ClassType::getMethods()
	{
		return this->methodList;
	}

	std::vector<Function*> ClassType::getMethodsWithName(const std::string& id)
	{
		auto l = this->classMethodMap[id];

		std::vector<Function*> ret;
		ret.reserve(l.size());

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


	bool ClassType::hasParent(Type* base)
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


	void ClassType::addTraitImpl(TraitType* trt)
	{
		if(zfu::contains(this->implTraits, trt))
			error("'%s' already implements trait '%s'", this, trt);

		this->implTraits.push_back(trt);
	}

	bool ClassType::implementsTrait(TraitType* trt)
	{
		return zfu::contains(this->implTraits, trt);
	}

	std::vector<TraitType*> ClassType::getImplementedTraits()
	{
		return this->implTraits;
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
		//* note: the 'reverse' virtual method map is to allow us, at translation time, to easily create the vtable without
		//* unnecessary searching. When we set a base class, we copy its 'reverse' map; thus, if we don't override anything,
		//* our vtable will just refer to the methods in the base class.

		//* but if we do override something, we just set the method in our 'reverse' map, which is what we'll use to build
		//* the vtable. simple?

		auto list = zfu::drop(method->getType()->toFunctionType()->getArgumentTypes(), 1);

		// check every member of the current mapping -- not the fastest method i admit.
		bool found = false;
		for(const auto& vm : this->virtualMethodMap)
		{
			if(vm.first.first == method->getName().name && areTypeListsContravariant(vm.first.second, list, /* trait checking: */ false))
			{
				found = true;
				this->virtualMethodMap[{ method->getName().name, list }] = vm.second;
				this->reverseVirtualMethodMap[vm.second] = method;
				break;
			}
		}

		if(!found)
		{
			// just make a new one.
			this->virtualMethodMap[{ method->getName().name, list }] = this->virtualMethodCount;
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
				name, ft, this->getTypeName().name);
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


	Function* ClassType::getInlineDestructor()
	{
		return this->inlineDestructor;
	}

	void ClassType::setInlineDestructor(Function* fn)
	{
		this->inlineDestructor = fn;
	}


	fir::Type* ClassType::substitutePlaceholders(const util::hash_map<fir::Type*, fir::Type*>& subst)
	{
		if(this->containsPlaceholders())
			error("not supported!");

		return this;
	}
}













