// ArrayCodegen.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "ast.h"
#include "codegen.h"

using namespace Ast;
using namespace Codegen;


Result_t ArrayIndex::codegen(CodegenInstance* cgi, fir::Value* lhsPtr, fir::Value* rhs)
{
	// get our array type
	fir::Type* atype = cgi->getExprType(this->arr);
	fir::Type* etype = nullptr;

	if(atype->isArrayType())
		etype = atype->toArrayType()->getElementType();

	else if(atype->isPointerType())
		etype = atype->getPointerElementType();

	else
		error(this, "Can only index on pointer or array types.");


	// try and do compile-time bounds checking
	if(atype->isArrayType())
	{
		fir::ArrayType* at = atype->toArrayType();

		// dynamic arrays don't get bounds checking
		if(at->getArraySize() != 0)
		{
			Number* n = nullptr;
			// todo: more robust
			if((n = dynamic_cast<Number*>(this->index)))
			{
				iceAssert(!n->decimal);
				if((uint64_t) n->ival >= at->getArraySize())
				{
					error(this, "'%zd' is out of bounds of array[%zd]", n->ival, at->getArraySize());
				}
			}
		}
	}

	// todo: bounds-check for pointers, allocated with 'alloc'.
	Result_t lhsp = this->arr->codegen(cgi);

	fir::Value* lhs = 0;
	if(lhsp.result.first->getType()->isPointerType())	lhs = lhsp.result.first;
	else												lhs = lhsp.result.second;

	fir::Value* gep = nullptr;
	fir::Value* ind = this->index->codegen(cgi).result.first;

	if(atype->isStructType() || atype->isArrayType())
	{
		gep = cgi->builder.CreateGEP2(lhs, fir::ConstantInt::getUint64(0), ind);
		// info(this, "lhs type: %s, gep type: %s\n", lhs->getType()->str().c_str(), gep->getType()->str().c_str());
	}
	else
	{
		gep = cgi->builder.CreateGetPointer(lhs, ind);
	}

	return Result_t(cgi->builder.CreateLoad(gep), gep);
}






































