// ArrayCodegen.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "../include/ast.h"
#include "../include/codegen.h"
#include "../include/llvm_all.h"

using namespace Ast;
using namespace Codegen;


Result_t ArrayIndex::codegen(CodegenInstance* cgi, llvm::Value* lhsPtr, llvm::Value* rhs)
{
	// get our array type
	llvm::Type* atype = cgi->getLlvmType(this->arr);
	llvm::Type* etype = nullptr;

	if(atype->isArrayTy())
		etype = llvm::cast<llvm::ArrayType>(atype)->getArrayElementType();

	else if(atype->isPointerTy())
		etype = atype->getPointerElementType();

	else
		error(cgi, this, "Can only index on pointer or array types.");


	// try and do compile-time bounds checking
	if(atype->isArrayTy())
	{
		iceAssert(llvm::isa<llvm::ArrayType>(atype));
		llvm::ArrayType* at = llvm::cast<llvm::ArrayType>(atype);

		// dynamic arrays don't get bounds checking
		if(at->getNumElements() != 0)
		{
			Number* n = nullptr;
			if((n = dynamic_cast<Number*>(this->index)))
			{
				iceAssert(!n->decimal);
				if((uint64_t) n->ival >= at->getNumElements())
				{
					error(cgi, this, "'%d' is out of bounds of array[%d]", n->ival, at->getNumElements());
				}
			}
		}
	}

	// todo: verify for pointers
	Result_t lhsp = this->arr->codegen(cgi);

	llvm::Value* lhs = 0;
	if(lhsp.result.first->getType()->isPointerTy())	lhs = lhsp.result.first;
	else											lhs = lhsp.result.second;

	llvm::Value* gep = nullptr;
	llvm::Value* ind = this->index->codegen(cgi).result.first;

	if(atype->isStructTy() || atype->isArrayTy())
	{
		llvm::Value* indices[2] = { llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(llvm::getGlobalContext()), 0), ind };
		gep = cgi->builder.CreateGEP(lhs, llvm::ArrayRef<llvm::Value*>(indices), "indexPtr");
	}
	else
	{
		gep = cgi->builder.CreateGEP(lhs, ind, "indexPtr");
	}

	// printf("array index: (%s, %s)\n", cgi->getReadableType(gep->getType()->getPointerElementType()).c_str(),
		// cgi->getReadableType(gep).c_str());

	return Result_t(cgi->builder.CreateLoad(gep), gep);
}
