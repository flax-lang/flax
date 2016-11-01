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
		return this->getType()->isGenericFunction();
	}

	Function* Function::reify(std::map<std::string, Type*> names, FTContext* tc)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		if(this->blocks.size() > 0)
			error("cannot reify already-generated function");

		FunctionType* newft = this->getType()->reify(names, tc);
		Function* nf = new Function(this->ident, newft, this->parentModule, this->linkageType);

		nf->setIsGenericInstantiation();
		return nf;
	}

	Function* Function::reifyUsingFunctionType(FunctionType* ft, FTContext* tc)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		if(ft->isGenericFunction())
			error("Cannot reify function using another parametric function type");

		// check that the function type is actually legit

		if(ft->getArgumentTypes().size() != this->getArguments().size())
			error("Mismatched number of arguments (%zu vs %zu)", this->getArguments().size(), ft->getArgumentTypes().size());


		// fuck, we need to go through all the arguments one by one -- make sure the types match
		std::map<std::string, Type*> foundTypes;

		for(size_t i = 0; i < this->fnArguments.size(); i++)
		{
			auto arg = this->fnArguments[i];
			if(arg->getType()->isParametricType())
			{
				if(foundTypes.find(arg->getType()->toParametricType()->getName()) != foundTypes.end()
					&& foundTypes[arg->getType()->toParametricType()->getName()] != ft->getArgumentN(i))
				{
					error("Mismatched argument (%zu) in reifying function of type '%s' with concrete type of '%s'; "
						"previously encountered type '%s' as '%s', found conflicting type '%s'", i, this->getType()->str().c_str(),
						ft->str().c_str(), arg->getType()->str().c_str(),
						foundTypes[arg->getType()->toParametricType()->getName()]->str().c_str(), ft->getArgumentN(i)->str().c_str());
				}

				foundTypes[arg->getType()->toParametricType()->getName()] = ft->getArgumentN(i);
			}
		}

		if(this->getReturnType()->isParametricType()
			&& foundTypes.find(this->getReturnType()->toParametricType()->getName()) != foundTypes.end()
			&& foundTypes[this->getReturnType()->toParametricType()->getName()] != ft->getReturnType())
		{
			error("Mismatched return type in reifying function of type '%s' with concrete type of '%s'; "
				"previously encountered type '%s' as '%s', found conflicting type '%s'", this->getType()->str().c_str(),
				ft->str().c_str(), this->getReturnType()->str().c_str(),
				foundTypes[this->getReturnType()->toParametricType()->getName()]->str().c_str(), ft->getReturnType()->str().c_str());
		}

		Function* nf = new Function(this->ident, ft, this->parentModule, this->linkageType);

		nf->setIsGenericInstantiation();
		return nf;
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



	bool Function::isGenericInstantiation()
	{
		return this->wasGenericInstantiation;
	}

	void Function::setIsGenericInstantiation()
	{
		this->wasGenericInstantiation = true;
	}

	bool Function::isAlwaysInlined()
	{
		return this->alwaysInlined;
	}

	void Function::setAlwaysInline()
	{
		this->alwaysInlined = true;
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































