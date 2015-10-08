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
	void CodegenInstance::addGlobalConstructor(std::string name, llvm::Function* constructor)
	{
		llvm::GlobalVariable* gv = this->module->getGlobalVariable(name);
		iceAssert(gv);

		this->globalConstructors.funcs[gv] = constructor;
	}

	void CodegenInstance::addGlobalConstructor(llvm::Value* ptr, llvm::Function* constructor)
	{
		iceAssert(ptr);
		llvm::Function* inhere = this->module->getFunction(constructor->getName());
		this->globalConstructors.funcs[ptr] = inhere;
	}

	void CodegenInstance::addGlobalConstructedValue(llvm::Value* ptr, llvm::Value* val)
	{
		iceAssert(ptr);
		iceAssert(val);

		this->globalConstructors.values[ptr] = val;
	}

	void CodegenInstance::addGlobalTupleConstructedValue(llvm::Value* ptr, int index, llvm::Value* val)
	{
		iceAssert(ptr);
		iceAssert(val);
		iceAssert(index >= 0);

		this->globalConstructors.tupleInitVals[ptr] = { index, val };
	}

	void CodegenInstance::addGlobalTupleConstructor(llvm::Value* ptr, int index, llvm::Function* func)
	{
		iceAssert(ptr);
		iceAssert(func);
		iceAssert(index >= 0);

		llvm::Function* inhere = this->module->getFunction(func->getName());
		this->globalConstructors.tupleInitFuncs[ptr] = { index, inhere };
	}

	void CodegenInstance::finishGlobalConstructors()
	{
		// generate initialiser
		llvm::FunctionType* ft = llvm::FunctionType::get(llvm::Type::getVoidTy(llvm::getGlobalContext()), false);
		llvm::Function* defaultInitFunc = llvm::Function::Create(ft, llvm::GlobalValue::ExternalLinkage, "__global_constructor__" + this->module->getModuleIdentifier(), this->module);

		llvm::BasicBlock* iblock = llvm::BasicBlock::Create(llvm::getGlobalContext(), "initialiser", defaultInitFunc);
		this->builder.SetInsertPoint(iblock);

		for(auto pair : this->globalConstructors.funcs)
			this->builder.CreateCall(pair.second, pair.first);

		for(auto pair : this->globalConstructors.values)
			this->builder.CreateStore(pair.second, pair.first);

		for(auto pair : this->globalConstructors.tupleInitFuncs)
		{
			std::pair<int, llvm::Function*> ivp = pair.second;

			llvm::Value* gep = this->builder.CreateStructGEP(pair.first, ivp.first);
			this->builder.CreateCall(ivp.second, gep);
		}

		for(auto pair : this->globalConstructors.tupleInitVals)
		{
			std::pair<int, llvm::Value*> ivp = pair.second;

			llvm::Value* gep = this->builder.CreateStructGEP(pair.first, ivp.first);
			this->builder.CreateStore(ivp.second, gep);
		}

		this->builder.CreateRetVoid();
		this->rootNode->globalConstructorTrampoline = defaultInitFunc;
	}
}









