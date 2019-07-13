// Function.cpp
// Copyright (c) 2014 - 2016, zhiayang
// Licensed under the Apache License Version 2.0.

#include "ir/module.h"
#include "ir/function.h"

#include <algorithm>

namespace fir
{
	// argument stuff
	Argument::Argument(Function* fn, Type* type) : Value(type)
	{
		this->parentFunction = fn;
		this->realValue = new Value(type);
	}

	Argument::~Argument()
	{
		delete this->realValue;
	}

	Value* Argument::getActualValue()
	{
		if(this->realValue) return this->realValue;
		error("calling getActualValue() when not in function! (no real value)");
	}

	Function* Argument::getParentFunction()
	{
		iceAssert(this->parentFunction);
		return this->parentFunction;
	}

	void Argument::setValue(Value* v)
	{
		iceAssert(v);
		this->realValue = v;
	}

	void Argument::clearValue()
	{
		this->realValue = 0;
	}













	// function stuff
	Function::Function(const Identifier& name, FunctionType* fnType, Module* module, LinkageType linkage)
		: GlobalValue(module, fnType, linkage)
	{
		this->ident = name;
		this->valueType = fnType;

		for(auto t : fnType->getArgumentTypes())
			this->fnArguments.push_back(new Argument(this, t));
	}

	Type* Function::getReturnType()
	{
		return this->getType()->getReturnType();
	}

	size_t Function::getArgumentCount()
	{
		return this->fnArguments.size();
	}

	const std::vector<Argument*>& Function::getArguments()
	{
		return this->fnArguments;
	}

	Argument* Function::getArgumentWithName(std::string name)
	{
		for(auto a : this->fnArguments)
		{
			if(a->getName().name == name)
				return a;
		}

		error("no argument named '%s' in function '%s'", name, this->getName().name);
	}

	bool Function::isCStyleVarArg()
	{
		return this->getType()->isCStyleVarArg();
	}

	bool Function::isVariadic()
	{
		return this->getType()->isVariadicFunc();
	}


	std::vector<IRBlock*>& Function::getBlockList()
	{
		return this->blocks;
	}

	void Function::deleteBody()
	{
		this->blocks.clear();
	}

	bool Function::isIntrinsicFunction()
	{
		return this->fnIsIntrinsicFunction;
	}

	void Function::setIsIntrinsic()
	{
		this->fnIsIntrinsicFunction = true;
	}

	void Function::addStackAllocation(Type* ty)
	{
		this->stackAllocs.push_back(ty);
	}

	const std::vector<Type*>& Function::getStackAllocations()
	{
		return this->stackAllocs;
	}


	void Function::cullUnusedValues()
	{
	}







	bool Function::wasDeclaredWithBodyElsewhere()
	{
		return this->hadBodyElsewhere;
	}

	void Function::setHadBodyElsewhere()
	{
		this->hadBodyElsewhere = true;
	}




	bool Function::isAlwaysInlined()
	{
		return this->alwaysInlined;
	}

	void Function::setAlwaysInline()
	{
		this->alwaysInlined = true;
	}









	Function* Function::create(const Identifier& name, fir::FunctionType* fnType, fir::Module* module, fir::LinkageType linkage)
	{
		return new Function(name, fnType, module, linkage);
	}




	// overridden stuff
	FunctionType* Function::getType()
	{
		FunctionType* ft = dcast(FunctionType, this->valueType);
		iceAssert(ft && "function is impostor (not valid function type)");

		return ft;
	}




















}































