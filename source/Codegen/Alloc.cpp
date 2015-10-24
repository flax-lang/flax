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


Result_t Alloc::codegen(CodegenInstance* cgi, fir::Value* lhsPtr, fir::Value* rhs)
{
	// if we haven't declared malloc() yet, then we need to do it here
	// NOTE: this is the only place in the compiler where a hardcoded call is made to a non-provided function.

	FuncPair_t* fp = cgi->getOrDeclareLibCFunc(MALLOC_FUNC);

	fir::Function* mallocf = fp->first;
	iceAssert(mallocf);

	mallocf = cgi->module->getFunction(mallocf->getName());
	iceAssert(mallocf);

	fir::Type* allocType = 0;

	allocType = cgi->getExprTypeFromStringType(this, this->type);
	iceAssert(allocType);





	// call malloc
	// todo: all broken

	fir::Value* oneValue = fir::ConstantInt::getUint64(1, cgi->getContext());
	fir::Value* zeroValue = fir::ConstantInt::getUint64(0, cgi->getContext());

	uint64_t typesize = cgi->execTarget->getTypeSizeInBits(allocType) / 8;
	fir::Value* allocsize = fir::ConstantInt::getUint64(typesize, cgi->getContext());
	fir::Value* allocnum = oneValue;


	fir::Value* isZero = nullptr;
	if(this->count)
	{
		allocnum = this->count->codegen(cgi).result.first;
		if(!allocnum->getType()->isIntegerType())
			error(this, "Expected integer type in alloc");

		allocnum = cgi->builder.CreateIntSizeCast(allocnum, allocsize->getType());
		allocsize = cgi->builder.CreateMul(allocsize, allocnum);


		// compare to zero. first see what we can do at compile time, if the constant is zero.
		if(dynamic_cast<fir::ConstantInt*>(allocnum))
		{
			fir::ConstantInt* ci = dynamic_cast<fir::ConstantInt*>(allocnum);
			if(ci->getSignedValue() == 0)
			{
				warn(this, "Allocating zero members with alloc[], will return null");
				isZero = fir::ConstantInt::getBool(true);
			}
		}
		else
		{
			// do it at runtime.
			isZero = cgi->builder.CreateICmpEQ(allocnum, fir::ConstantInt::getNullValue(allocsize->getType()));
		}
	}

	fir::Value* allocmemptr = lhsPtr ? lhsPtr : cgi->allocateInstanceInBlock(allocType->getPointerTo());

	fir::Value* amem = cgi->builder.CreatePointerTypeCast(cgi->builder.CreateCall1(mallocf, allocsize), allocType->getPointerTo());

	cgi->builder.CreateStore(amem, allocmemptr);
	fir::Value* allocatedmem = cgi->builder.CreateLoad(allocmemptr);

	// call the initialiser, if there is one
	if(allocType->isIntegerType() || allocType->isPointerType())
	{
		// fir::Value* cs = cgi->builder.CreateBitcast(allocatedmem, fir::PointerType::getInt8Ptr(cgi->getContext()));
		// fir::Value* dval = fir::ConstantValue::getNullValue(cs->getType()->getPointerElementType());

		// printf("%s, %s, %s, %llu\n", cgi->getReadableType(cs).c_str(), cgi->getReadableType(dval).c_str(),
			// cgi->getReadableType(allocsize).c_str(), typesize);

		// cgi->builder.CreateMemSet(cs, dval, allocsize, typesize);
	}
	else
	{
		TypePair_t* typePair = 0;

		std::vector<fir::Value*> args;
		args.push_back(allocatedmem);
		for(Expr* e : this->params)
			args.push_back(e->codegen(cgi).result.first);

		typePair = cgi->getType(allocType);

		fir::Function* initfunc = cgi->getStructInitialiser(this, typePair, args);

		// we need to keep calling this... essentially looping.
		fir::IRBlock* curbb = cgi->builder.getCurrentBlock();	// store the current bb
		fir::IRBlock* loopBegin = cgi->builder.addNewBlockInFunction("loopBegin", curbb->getParentFunction());
		fir::IRBlock* loopEnd = cgi->builder.addNewBlockInFunction("loopEnd", curbb->getParentFunction());
		fir::IRBlock* after = cgi->builder.addNewBlockInFunction("afterLoop", curbb->getParentFunction());



		// check for zero.
		if(isZero)
		{
			fir::IRBlock* notZero = cgi->builder.addNewBlockInFunction("notZero", curbb->getParentFunction());
			fir::IRBlock* setToZero = cgi->builder.addNewBlockInFunction("zeroAlloc", curbb->getParentFunction());
			cgi->builder.CreateCondBranch(isZero, setToZero, notZero);

			cgi->builder.setCurrentBlock(setToZero);
			cgi->builder.CreateStore(fir::ConstantValue::getNullValue(allocatedmem->getType()), allocmemptr);
			allocatedmem = cgi->builder.CreateLoad(allocmemptr);
			cgi->builder.CreateUnCondBranch(after);

			cgi->builder.setCurrentBlock(notZero);
		}









		// create the loop counter (initialise it with the value)
		fir::Value* counterptr = cgi->allocateInstanceInBlock(allocsize->getType());
		cgi->builder.CreateStore(allocnum, counterptr);

		// do { ...; num--; } while(num - 1 > 0)
		cgi->builder.CreateUnCondBranch(loopBegin);	// explicit branch


		// start in the loop
		cgi->builder.setCurrentBlock(loopBegin);

		// call the constructor
		allocatedmem = cgi->builder.CreateLoad(allocmemptr);
		args[0] = allocatedmem;
		cgi->builder.CreateCall(initfunc, args);

		// move the allocatedmem pointer by the type size
		cgi->doPointerArithmetic(ArithmeticOp::Add, allocatedmem, allocmemptr, oneValue);
		allocatedmem = cgi->builder.CreateLoad(allocmemptr);

		// subtract the counter
		fir::Value* counter = cgi->builder.CreateLoad(counterptr);
		cgi->builder.CreateStore(cgi->builder.CreateSub(counter, oneValue), counterptr);

		// do the comparison
		counter = cgi->builder.CreateLoad(counterptr);

		fir::Value* brcond = cgi->builder.CreateICmpGT(counter, zeroValue);
		cgi->builder.CreateCondBranch(brcond, loopBegin, loopEnd);

		// at loopend:
		cgi->builder.setCurrentBlock(loopEnd);

		// undo the pointer additions we did above
		cgi->doPointerArithmetic(ArithmeticOp::Subtract, allocatedmem, allocmemptr, allocnum);

		allocatedmem = cgi->builder.CreateLoad(allocmemptr);

		cgi->doPointerArithmetic(ArithmeticOp::Add, allocatedmem, allocmemptr, oneValue);


		cgi->builder.CreateUnCondBranch(after);
		cgi->builder.setCurrentBlock(after);
		allocatedmem = cgi->builder.CreateLoad(allocmemptr);
	}

	return Result_t(allocatedmem, 0);
}


Result_t Dealloc::codegen(CodegenInstance* cgi, fir::Value* lhsPtr, fir::Value* rhs)
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





















