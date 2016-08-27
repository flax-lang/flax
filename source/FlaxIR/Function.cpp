// Function.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ir/module.h"
#include "ir/function.h"

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
		iceAssert(0 && "Calling getActualValue() when not in function! (no real value)");
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
	Function::Function(Identifier name, FunctionType* fnType, Module* module, LinkageType linkage) : GlobalValue(module, fnType, linkage)
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

	std::deque<Argument*> Function::getArguments()
	{
		return this->fnArguments;
	}

	bool Function::isCStyleVarArg()
	{
		return this->getType()->isCStyleVarArg();
	}

	bool Function::isVariadic()
	{
		return this->getType()->isVariadicFunc();
	}


	std::deque<IRBlock*>& Function::getBlockList()
	{
		return this->blocks;
	}

	void Function::deleteBody()
	{
		this->blocks.clear();
	}

	bool Function::isGeneric()
	{
		for(auto p : this->fnArguments)
		{
			if(p->getType()->isParametricType())
				return true;
		}

		return this->getReturnType()->isParametricType();
	}

	Function* Function::reify(std::map<std::string, Type*> names, FTContext* tc)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");


		if(this->blocks.size() > 0)
			error("cannot reify already-generated function");

		FunctionType* newft = this->getType()->reify(names, tc);
		Function* nf = new Function(this->ident, newft, this->parentModule, this->linkageType);

		return nf;
	}

	Function* Function::create(Identifier name, fir::FunctionType* fnType, fir::Module* module, fir::LinkageType linkage)
	{
		return new Function(name, fnType, module, linkage);
	}




	// overridden stuff
	FunctionType* Function::getType()
	{
		FunctionType* ft = dynamic_cast<FunctionType*>(this->valueType);
		iceAssert(ft && "Function is impostor (not valid function type)");

		return ft;
	}
}































