// GlobalConstructors.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "ast.h"
#include "codegen.h"
#include "llvm_all.h"

using namespace Ast;
using namespace Codegen;

namespace Codegen
{
	void CodegenInstance::addGlobalConstructor(std::string name, llvm::Function* constructor)
	{
		llvm::GlobalVariable* gv = this->mainModule->getGlobalVariable(name);
		iceAssert(gv);

		this->globalConstructors[gv] = constructor;
	}

	void CodegenInstance::finishGlobalConstructors()
	{
		// generate initialiser
		llvm::FunctionType* ft = llvm::FunctionType::get(llvm::Type::getVoidTy(llvm::getGlobalContext()), false);
		llvm::Function* defaultInitFunc = llvm::Function::Create(ft, llvm::GlobalValue::ExternalLinkage, "__global_constructor__" + this->mainModule->getModuleIdentifier(), this->mainModule);

		llvm::BasicBlock* iblock = llvm::BasicBlock::Create(llvm::getGlobalContext(), "initialiser", defaultInitFunc);
		this->mainBuilder.SetInsertPoint(iblock);

		for(auto pair : this->globalConstructors)
		{
			this->mainBuilder.CreateCall(pair.second, pair.first);
		}

		this->mainBuilder.CreateRetVoid();
		this->rootNode->globalConstructorTrampoline = defaultInitFunc;
	}
}
