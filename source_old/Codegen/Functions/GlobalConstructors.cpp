// GlobalConstructors.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "ast.h"
#include "codegen.h"

using namespace Ast;
using namespace Codegen;

namespace Codegen
{
	static size_t id = 0;
	fir::Function* CodegenInstance::procureAnonymousConstructorFunction(fir::Value* arg)
	{
		auto ident = Identifier(this->module->getModuleName() + "_anon_constr_" + std::to_string(id++), IdKind::ModuleConstructor);

		fir::Function* func = this->module->getOrCreateFunction(ident, fir::FunctionType::get({ arg->getType() },
			fir::Type::getVoid(), false), fir::LinkageType::Internal);

		// basically returns a "ready-to-use" function
		fir::IRBlock* entry = new fir::IRBlock(func);

		entry->setName("entry");
		func->getBlockList().push_back(entry);

		return func;
	}




	void CodegenInstance::addGlobalConstructor(const Identifier& id, fir::Function* constructor)
	{
		fir::GlobalVariable* gv = this->module->getGlobalVariable(id);
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

	void CodegenInstance::finishGlobalConstructors()
	{
		// generate initialiser
		fir::FunctionType* ft = fir::FunctionType::get({ }, fir::Type::getVoid(), false);
		auto id = Identifier(this->module->getModuleName(), IdKind::ModuleConstructor);
		id.functionArguments = ft->getArgumentTypes();

		fir::Function* defaultInitFunc = this->module->getOrCreateFunction(Identifier(id.str() + "_constr", IdKind::ModuleConstructor),
			ft, fir::LinkageType::External);

		fir::IRBlock* iblock = this->irb.addNewBlockInFunction("initialiser", defaultInitFunc);
		this->irb.setCurrentBlock(iblock);


		for(auto pair : this->globalConstructors.funcs)
		{
			pair.first->makeNotImmutable();

			this->irb.CreateCall1(pair.second, pair.first);

			pair.first->makeImmutable();
		}

		for(auto pair : this->globalConstructors.values)
		{
			fir::Value* gv = pair.first;
			fir::Value* val = pair.second;

			pair.first->makeNotImmutable();

			val = this->autoCastType(gv->getType()->getPointerElementType(), val);
			this->irb.CreateStore(val, gv);

			pair.first->makeImmutable();
		}

		this->irb.CreateReturnVoid();
		this->rootNode->globalConstructorTrampoline = defaultInitFunc;
	}
}









