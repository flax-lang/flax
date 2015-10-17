// GlobalConstructors.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "ast.h"
#include "codegen.h"

#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"

using namespace Ast;
using namespace Codegen;

namespace Codegen
{
	void CodegenInstance::addGlobalConstructor(std::string name, fir::Function* constructor)
	{
		fir::GlobalVariable* gv = this->module->getGlobalVariable(name);
		iceAssert(gv);

		this->globalConstructors.funcs[gv] = constructor;
	}

	void CodegenInstance::addGlobalConstructor(fir::Value* ptr, fir::Function* constructor)
	{
		iceAssert(ptr);
		fir::Function* inhere = this->module->getFunction(constructor->getName());
		this->globalConstructors.funcs[ptr] = inhere;
	}

	void CodegenInstance::addGlobalConstructedValue(fir::Value* ptr, fir::Value* val)
	{
		iceAssert(ptr);
		iceAssert(val);

		this->globalConstructors.values[ptr] = val;
	}

	void CodegenInstance::addGlobalTupleConstructedValue(fir::Value* ptr, int index, fir::Value* val)
	{
		iceAssert(ptr);
		iceAssert(val);
		iceAssert(index >= 0);

		this->globalConstructors.tupleInitVals[ptr] = { index, val };
	}

	void CodegenInstance::addGlobalTupleConstructor(fir::Value* ptr, int index, fir::Function* func)
	{
		iceAssert(ptr);
		iceAssert(func);
		iceAssert(index >= 0);

		fir::Function* inhere = this->module->getFunction(func->getName());
		this->globalConstructors.tupleInitFuncs[ptr] = { index, inhere };
	}

	void CodegenInstance::finishGlobalConstructors()
	{
		// generate initialiser
		fir::FunctionType* ft = fir::FunctionType::get({ }, fir::PrimitiveType::getVoid(fir::getDefaultFTContext()), false);
		fir::Function* defaultInitFunc = new fir::Function("__global_constructor__" + this->module->getModuleName(), ft,
			this->module, fir::LinkageType::External);

		fir::IRBlock* iblock = this->builder.addNewBlockInFunction("initialiser", defaultInitFunc);
		this->builder.setCurrentBlock(iblock);

		for(auto pair : this->globalConstructors.funcs)
			this->builder.CreateCall1(pair.second, pair.first);

		for(auto pair : this->globalConstructors.values)
			this->builder.CreateStore(pair.second, pair.first);

		for(auto pair : this->globalConstructors.tupleInitFuncs)
		{
			std::pair<int, fir::Function*> ivp = pair.second;

			fir::Value* gep = this->builder.CreateGetConstStructMember(pair.first, ivp.first);
			// fir::Value* gep = this->builder.CreateStructGEP(pair.first, ivp.first);
			this->builder.CreateCall1(ivp.second, gep);
		}

		for(auto pair : this->globalConstructors.tupleInitVals)
		{
			std::pair<int, fir::Value*> ivp = pair.second;

			fir::Value* gep = this->builder.CreateGetConstStructMember(pair.first, ivp.first);
			// fir::Value* gep = this->builder.CreateStructGEP(pair.first, ivp.first);
			this->builder.CreateStore(ivp.second, gep);
		}

		this->builder.CreateReturnVoid();
		this->rootNode->globalConstructorTrampoline = defaultInitFunc;
	}
}









