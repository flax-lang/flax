// GlobalConstructors.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "ast.h"
#include "codegen.h"

using namespace Ast;
using namespace Codegen;

namespace Codegen
{
	void CodegenInstance::addGlobalConstructor(Identifier id, fir::Function* constructor)
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
		fir::FunctionType* ft = fir::FunctionType::get({ }, fir::PrimitiveType::getVoid(), false);
		auto id = Identifier(this->module->getModuleName(), IdKind::ModuleConstructor);
		id.functionArguments = ft->getArgumentTypes();

		fir::Function* defaultInitFunc = this->module->getOrCreateFunction(id, ft, fir::LinkageType::External);

		fir::IRBlock* iblock = this->builder.addNewBlockInFunction("initialiser", defaultInitFunc);
		this->builder.setCurrentBlock(iblock);


		for(auto pair : this->globalConstructors.funcs)
		{
			pair.first->makeNotImmutable();

			this->builder.CreateCall1(pair.second, pair.first);

			pair.first->makeImmutable();
		}

		for(auto pair : this->globalConstructors.values)
		{
			fir::Value* gv = pair.first;
			fir::Value* val = pair.second;

			pair.first->makeNotImmutable();

			val = this->autoCastType(gv->getType()->getPointerElementType(), val);
			this->builder.CreateStore(val, gv);

			pair.first->makeImmutable();
		}

		for(auto pair : this->globalConstructors.tupleInitFuncs)
		{
			std::pair<int, fir::Function*> ivp = pair.second;


			pair.first->makeNotImmutable();

			fir::Value* gep = this->builder.CreateStructGEP(pair.first, ivp.first);
			this->builder.CreateCall1(ivp.second, gep);

			pair.first->makeImmutable();
		}

		for(auto pair : this->globalConstructors.tupleInitVals)
		{
			std::pair<int, fir::Value*> ivp = pair.second;

			pair.first->makeNotImmutable();

			fir::Value* gep = this->builder.CreateStructGEP(pair.first, ivp.first);
			this->builder.CreateStore(ivp.second, gep);

			pair.first->makeImmutable();
		}

		this->builder.CreateReturnVoid();
		this->rootNode->globalConstructorTrampoline = defaultInitFunc;
	}
}









