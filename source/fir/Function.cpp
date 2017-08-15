// Function.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
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

	std::vector<Argument*> Function::getArguments()
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


	std::vector<IRBlock*>& Function::getBlockList()
	{
		return this->blocks;
	}

	void Function::deleteBody()
	{
		this->blocks.clear();
	}




	#if 0
	static void _recursivelyRemoveInstruction(Instruction* instr)
	{
		if(instr->hasSideEffects())
			return;

		for(auto v : instr->operands)
		{
			// if we are the only user, basically
			if(v->getUsers().size() == 1 && v->getUsers()[0] == instr && v->getSource())
			{
				Instruction* src = v->getSource();
				_recursivelyRemoveInstruction(src);
			}
		}

		// ok
		auto& list = instr->parentBlock->getInstructions();

		auto it = std::find(list.begin(), list.end(), instr);
		iceAssert(it != list.end());
		list.erase(it);
	}


	static void recursivelyRemoveInstruction(Instruction* instr)
	{
		for(auto v : instr->operands)
		{
			// if we are the only user, basically
			if(v->getUsers().size() == 1 && v->getUsers()[0] == instr && v->getSource())
			{
				Instruction* src = v->getSource();
				_recursivelyRemoveInstruction(src);
			}
		}

		delete instr;
	}
	#endif



	void Function::cullUnusedValues()
	{
		#if 0
		for(auto b : this->blocks)
		{
			auto& instrs = b->getInstructions();
			size_t i = 0;
			for(auto it = instrs.begin(); it != instrs.end(); i++)
			{
				if((*it)->realOutput->getUsers().empty() && !(*it)->hasSideEffects())
				{
					Instruction* instr = *it;
					it = instrs.erase(it);

					recursivelyRemoveInstruction(instr);
				}
				else
				{
					it++;
				}
			}
		}
		#endif
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
		FunctionType* ft = dynamic_cast<FunctionType*>(this->valueType);
		iceAssert(ft && "Function is impostor (not valid function type)");

		return ft;
	}




















}































