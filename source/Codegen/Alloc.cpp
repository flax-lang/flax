// AllocCodegen.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "codegen.h"

using namespace Ast;
using namespace Codegen;


#define MALLOC_FUNC		"malloc"
#define FREE_FUNC		"free"
#define MEMSET_FUNC		"memset"



static fir::Value* recursivelyDoAlloc(CodegenInstance* cgi, fir::Type* type, fir::Value* size, std::deque<Expr*> params,
	std::deque<fir::Value*>& sizes)
{
	FuncPair_t* fp = cgi->getOrDeclareLibCFunc(MALLOC_FUNC);

	fir::Function* mallocf = fp->first;
	iceAssert(mallocf);

	mallocf = cgi->module->getFunction(mallocf->getName());
	iceAssert(mallocf);


	fir::Value* oneValue = fir::ConstantInt::getUint64(1, cgi->getContext());
	fir::Value* zeroValue = fir::ConstantInt::getUint64(0, cgi->getContext());

	uint64_t typesize = cgi->execTarget->getTypeSizeInBits(type) / 8;
	fir::Value* allocsize = fir::ConstantInt::getUint64(typesize, cgi->getContext());

	size = cgi->autoCastType(allocsize->getType(), size);

	fir::Value* totalAlloc = cgi->builder.CreateMul(allocsize, size, "totalalloc");
	fir::Value* allocmemptr = cgi->getStackAlloc(type->getPointerTo(), "allocmemptr");

	fir::Value* amem = cgi->builder.CreatePointerTypeCast(cgi->builder.CreateCall1(mallocf, totalAlloc), type->getPointerTo());
	cgi->builder.CreateStore(amem, allocmemptr);



	fir::IRBlock* curbb = cgi->builder.getCurrentBlock();	// store the current bb
	fir::IRBlock* loopHead = cgi->builder.addNewBlockInFunction("loopHead", curbb->getParentFunction());
	fir::IRBlock* loopBegin = cgi->builder.addNewBlockInFunction("loopBegin", curbb->getParentFunction());
	fir::IRBlock* loopEnd = cgi->builder.addNewBlockInFunction("loopEnd", curbb->getParentFunction());

	fir::IRBlock* allocZeroCase = cgi->builder.addNewBlockInFunction("allocZeroCase", curbb->getParentFunction());
	fir::IRBlock* loopMerge = cgi->builder.addNewBlockInFunction("loopMerge", curbb->getParentFunction());

	cgi->builder.setCurrentBlock(curbb);
	cgi->builder.CreateUnCondBranch(loopHead);

	cgi->builder.setCurrentBlock(loopHead);

	// set a counter
	fir::Value* counterPtr = cgi->builder.CreateStackAlloc(fir::PrimitiveType::getUint64(), "counterPtr");
	cgi->builder.CreateStore(zeroValue, counterPtr);

	// check for zero.
	{
		fir::Value* isZero = cgi->builder.CreateICmpEQ(size, zeroValue, "iszero");
		cgi->builder.CreateCondBranch(isZero, allocZeroCase, loopBegin);
	}


	// begin the loop
	cgi->builder.setCurrentBlock(loopBegin);
	{
		// get the pointer.
		fir::Value* pointer = cgi->builder.CreateGetPointer(cgi->builder.CreateLoad(allocmemptr),
			cgi->builder.CreateLoad(counterPtr), "pointerPtr");

		if(type->isStructType())
		{
			// call the init func
			TypePair_t* typePair = 0;

			std::vector<fir::Value*> args;
			args.push_back(pointer);
			for(Expr* e : params)
				args.push_back(e->codegen(cgi).result.first);

			typePair = cgi->getType(type);
			fir::Function* initfunc = cgi->getStructInitialiser(/* user */ 0, typePair, args);
			iceAssert(initfunc);

			cgi->builder.CreateCall(initfunc, args);
		}
		else if(type->isPointerType() && sizes.size() > 0)
		{
			fir::Value* front = sizes.front();
			sizes.pop_front();

			fir::Value* rret = recursivelyDoAlloc(cgi, type->getPointerElementType(), front, params, sizes);
			cgi->builder.CreateStore(rret, pointer);
		}
		else
		{
			cgi->builder.CreateStore(fir::ConstantValue::getNullValue(type), pointer);
		}


		// increment counter
		fir::Value* incremented = cgi->builder.CreateAdd(oneValue, cgi->builder.CreateLoad(counterPtr));
		cgi->builder.CreateStore(incremented, counterPtr);


		// check.
		// basically this: if counter < size goto loopBegin else goto loopEnd

		cgi->builder.CreateCondBranch(cgi->builder.CreateICmpLT(cgi->builder.CreateLoad(counterPtr), size), loopBegin, loopEnd);
	}


	// in zeroBlock, set the pointer to 0 and branch to merge
	cgi->builder.setCurrentBlock(allocZeroCase);
	{
		cgi->builder.CreateStore(fir::ConstantValue::getNullValue(type->getPointerTo()), allocmemptr);
		cgi->builder.CreateUnCondBranch(loopMerge);
	}



	// in end block... do nothing.
	cgi->builder.setCurrentBlock(loopEnd);
	{
		cgi->builder.CreateUnCondBranch(loopMerge);
	}


	cgi->builder.setCurrentBlock(loopMerge);
	fir::Value* ret = cgi->builder.CreateLoad(allocmemptr, "mem");

	return ret;
}























Result_t Alloc::codegen(CodegenInstance* cgi, fir::Value* extra)
{
	// if we haven't declared malloc() yet, then we need to do it here
	// NOTE: this is the only place in the compiler where a hardcoded call is made to a non-provided function.

	fir::Type* allocType = 0;

	allocType = cgi->getExprTypeFromStringType(this, this->type);
	iceAssert(allocType);




	// call malloc


	fir::Value* oneValue = fir::ConstantInt::getUint64(1, cgi->getContext());
	// fir::Value* zeroValue = fir::ConstantInt::getUint64(0, cgi->getContext());


	if(this->counts.size() > 0)
	{
		std::deque<fir::Value*> cnts;

		for(auto c : this->counts)
			cnts.push_back(c->codegen(cgi).result.first);

		fir::Value* firstSize = cnts.front();
		cnts.pop_front();


		for(size_t i = 1; i < this->counts.size(); i++)
			allocType = allocType->getPointerTo();


		// this is the pointer.
		// in accordance with new things, create a LowLevelVariableArray value, consisting of a pointer and a length.

		fir::Value* ret = recursivelyDoAlloc(cgi, allocType, firstSize, this->params, cnts);
		return Result_t(ret, 0);
	}
	else
	{
		std::deque<fir::Value*> cnts;

		fir::Value* ret = recursivelyDoAlloc(cgi, allocType, oneValue, this->params, cnts);
		return Result_t(ret, 0);
	}
}





Result_t Dealloc::codegen(CodegenInstance* cgi, fir::Value* extra)
{
	fir::Value* freearg = 0;
	if(dynamic_cast<VarRef*>(this->expr))
	{
		SymbolPair_t* sp = cgi->getSymPair(this, dynamic_cast<VarRef*>(this->expr)->name);
		if(!sp)
			error(this, "Unknown symbol '%s'", dynamic_cast<VarRef*>(this->expr)->name.c_str());


		// this will be an alloca instance (aka pointer to whatever type it actually was)
		fir::Value* varval = sp->first;

		// therefore, create a Load to get the actual value
		varval = cgi->builder.CreateLoad(varval);
		freearg = cgi->builder.CreatePointerTypeCast(varval, fir::PointerType::getInt8Ptr(cgi->getContext()));
	}
	else
	{
		freearg = this->expr->codegen(cgi).result.first;
	}

	// call 'free'
	FuncPair_t* fp = cgi->getOrDeclareLibCFunc(FREE_FUNC);


	fir::Function* freef = fp->first;
	iceAssert(freef);

	freef = cgi->module->getFunction(freef->getName());
	iceAssert(freef);


	cgi->builder.CreateCall1(freef, freearg);
	return Result_t(0, 0);
}





















