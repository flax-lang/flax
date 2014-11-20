// VarCodegen.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "../include/ast.h"
#include "../include/codegen.h"
#include "../include/llvm_all.h"

using namespace Ast;
using namespace Codegen;


ValPtr_p VarRef::codeGen()
{
	llvm::Value* val = getSymInst(this->name);
	if(!val)
		error("Unknown variable name '%s'", this->name.c_str());

	return ValPtr_p(mainBuilder.CreateLoad(val, this->name), val);
}

ValPtr_p VarDecl::codeGen()
{
	if(isDuplicateSymbol(this->name))
		error("Redefining duplicate symbol '%s'", this->name.c_str());

	llvm::Function* func = mainBuilder.GetInsertBlock()->getParent();
	llvm::Value* val = nullptr;

	llvm::AllocaInst* ai = allocateInstanceInBlock(func, this);
	getSymTab()[this->name] = std::pair<llvm::AllocaInst*, VarDecl*>(ai, this);

	if(this->initVal)
	{
		this->initVal = autoCastType(this, this->initVal);
		val = this->initVal->codeGen().first;
	}
	else if(isBuiltinType(this) || isArrayType(this))
	{
		val = getDefaultValue(this);
	}
	else
	{
		// get our type
		TypePair_t* pair = getType(this->type);
		if(!pair)
			error("Invalid type");

		if(pair->first->isStructTy())
		{
			assert(pair->second.second == ExprType::Struct);
			assert(pair->second.first);

			Struct* str = nullptr;
			assert((str = dynamic_cast<Struct*>(pair->second.first)));

			val = mainBuilder.CreateCall(str->initFunc, ai);

			// don't do the store, they return void
			return ValPtr_p(val, ai);
		}
		else
		{
			error("Unknown type encountered");
		}
	}

	mainBuilder.CreateStore(val, ai);
	return ValPtr_p(val, ai);
}














