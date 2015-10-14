// ArrayCodegen.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "../include/ast.h"
#include "../include/codegen.h"

using namespace Ast;
using namespace Codegen;


Result_t ArrayIndex::codegen(CodegenInstance* cgi, fir::Value* lhsPtr, fir::Value* rhs)
{
	// get our array type
	fir::Type* atype = cgi->getLlvmType(this->arr);
	fir::Type* etype = nullptr;

	if(atype->isArrayTy())
		etype = fir::cast<fir::ArrayType>(atype)->getArrayElementType();

	else if(atype->isPointerTy())
		etype = atype->getPointerElementType();

	else
		error(this, "Can only index on pointer or array types.");


	// try and do compile-time bounds checking
	if(atype->isArrayTy())
	{
		iceAssert(fir::isa<fir::ArrayType>(atype));
		fir::ArrayType* at = fir::cast<fir::ArrayType>(atype);

		// dynamic arrays don't get bounds checking
		if(at->getNumElements() != 0)
		{
			Number* n = nullptr;
			if((n = dynamic_cast<Number*>(this->index)))
			{
				iceAssert(!n->decimal);
				if((uint64_t) n->ival >= at->getNumElements())
				{
					error(this, "'%zd' is out of bounds of array[%zd]", n->ival, at->getNumElements());
				}
			}
		}
	}

	// todo: bounds-check for pointers, allocated with 'alloc'.
	Result_t lhsp = this->arr->codegen(cgi);

	fir::Value* lhs = 0;
	if(lhsp.result.first->getType()->isPointerTy())	lhs = lhsp.result.first;
	else											lhs = lhsp.result.second;

	fir::Value* gep = nullptr;
	fir::Value* ind = this->index->codegen(cgi).result.first;

	if(atype->isStructTy() || atype->isArrayTy())
	{
		fir::Value* indices[2] = { fir::ConstantInt::get(fir::IntegerType::getInt32Ty(fir::getGlobalContext()), 0), ind };
		gep = cgi->builder.CreateGEP(lhs, fir::ArrayRef<fir::Value*>(indices), "indexPtr");
	}
	else
	{
		gep = cgi->builder.CreateGEP(lhs, ind, "indexPtr");
	}

	// printf("array index: (%s, %s)\n", cgi->getReadableType(gep->getType()->getPointerElementType()).c_str(),
		// cgi->getReadableType(gep).c_str());

	return Result_t(cgi->builder.CreateLoad(gep), gep);
}
