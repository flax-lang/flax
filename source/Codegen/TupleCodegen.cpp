// TupleCodegen.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "../include/ast.h"
#include "../include/codegen.h"
#include "../include/llvm_all.h"

using namespace Ast;
using namespace Codegen;


llvm::StructType* Tuple::getType(CodegenInstance* cgi)
{
	if(this->ltypes.size() == 0)
	{
		iceAssert(!this->didCreateType);

		for(Expr* e : this->values)
			this->ltypes.push_back(cgi->getLlvmType(e));

		this->name = "__anonymoustuple_" + std::to_string(cgi->typeMap.size());
		this->cachedLlvmType = llvm::StructType::create(cgi->getContext(), this->ltypes, this->name);
		this->didCreateType = true;

		// todo: debate, should we add this?
		cgi->addNewType(this->cachedLlvmType, this, TypeKind::Tuple);
	}

	return this->cachedLlvmType;
}

void Tuple::createType(CodegenInstance* cgi)
{
	(void) cgi;
	return;
}

Result_t Tuple::codegen(CodegenInstance* cgi, llvm::Value* lhsPtr, llvm::Value* rhs)
{
	(void) rhs;

	llvm::Value* gep = 0;
	if(lhsPtr)
	{
		gep = lhsPtr;
	}
	else
	{
		gep = cgi->allocateInstanceInBlock(this->getType(cgi));
	}

	llvm::Type* strtype = gep->getType()->getPointerElementType();

	iceAssert(gep);
	iceAssert(strtype->isStructTy());

	// set all the values.
	// do the gep for each.

	iceAssert(strtype->getStructNumElements() == this->values.size());

	for(unsigned int i = 0; i < strtype->getStructNumElements(); i++)
	{
		llvm::Value* member = cgi->mainBuilder.CreateStructGEP(gep, i);
		llvm::Value* val = this->values[i]->codegen(cgi).result.first;

		cgi->mainBuilder.CreateStore(val, member);
	}

	return Result_t(cgi->mainBuilder.CreateLoad(gep), gep);
}



















