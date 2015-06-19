// AllocCodegen.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "../include/ast.h"
#include "../include/codegen.h"
#include "../include/llvm_all.h"

using namespace Ast;
using namespace Codegen;


#define MALLOC_FUNC		"malloc"
#define FREE_FUNC		"free"


Result_t Alloc::codegen(CodegenInstance* cgi, llvm::Value* lhsPtr, llvm::Value* rhs)
{
	// if we haven't declared malloc() yet, then we need to do it here
	// NOTE: this is the only place in the compiler where a hardcoded call is made to a non-provided function.

	FuncPair_t* fp = cgi->getOrDeclareLibCFunc(MALLOC_FUNC);

	llvm::Function* mallocf = fp->first;
	iceAssert(mallocf);

	llvm::Type* allocType = 0;

	allocType = cgi->getLlvmType(this, this->type);
	iceAssert(allocType);


	llvm::Value* oneValue = llvm::ConstantInt::get(llvm::IntegerType::getInt64Ty(cgi->getContext()), 1);
	llvm::Value* zeroValue = llvm::ConstantInt::get(llvm::IntegerType::getInt64Ty(cgi->getContext()), 0);



	// call malloc
	uint64_t typesize = cgi->module->getDataLayout()->getTypeSizeInBits(allocType) / 8;
	llvm::Value* allocsize = llvm::ConstantInt::get(llvm::IntegerType::getInt64Ty(cgi->getContext()), typesize);
	llvm::Value* allocnum = oneValue;


	llvm::Value* isZero = nullptr;
	if(this->count)
	{
		allocnum = this->count->codegen(cgi).result.first;
		if(!allocnum->getType()->isIntegerTy())
			error(cgi, this, "Expected integer type in alloc");

		allocnum = cgi->builder.CreateIntCast(allocnum, allocsize->getType(), false);
		allocsize = cgi->builder.CreateMul(allocsize, allocnum);


		// compare to zero. first see what we can do at compile time, if the constant is zero.
		if(llvm::isa<llvm::ConstantInt>(allocnum))
		{
			llvm::ConstantInt* ci = llvm::cast<llvm::ConstantInt>(allocnum);
			if(ci->isZeroValue())
			{
				warn(this, "Allocating zero members with alloc[], will return null");
				isZero = llvm::ConstantInt::getTrue(cgi->getContext());
			}
		}
		else
		{
			// do it at runtime.
			isZero = cgi->builder.CreateICmpEQ(allocnum, llvm::ConstantInt::getNullValue(allocsize->getType()));
		}
	}

	llvm::Value* allocmemptr = lhsPtr ? lhsPtr : cgi->allocateInstanceInBlock(allocType->getPointerTo());

	llvm::Value* amem = cgi->builder.CreatePointerCast(cgi->builder.CreateCall(mallocf, allocsize), allocType->getPointerTo());
	// warn(cgi, this, "%s -> %s\n", cgi->getReadableType(amem).c_str(), cgi->getReadableType(allocmemptr).c_str());

	cgi->builder.CreateStore(amem, allocmemptr);
	llvm::Value* allocatedmem = cgi->builder.CreateLoad(allocmemptr);

	// call the initialiser, if there is one
	if(allocType->isIntegerTy() || allocType->isPointerTy())
	{
		llvm::Value* defaultValue = 0;
		defaultValue = llvm::Constant::getNullValue(allocType);
		cgi->builder.CreateMemSet(allocatedmem, defaultValue, allocsize, typesize);
	}
	else
	{
		TypePair_t* typePair = 0;

		std::vector<llvm::Value*> args;
		args.push_back(allocatedmem);
		for(Expr* e : this->params)
			args.push_back(e->codegen(cgi).result.first);

		typePair = cgi->getType(allocType);

		llvm::Function* initfunc = cgi->getStructInitialiser(this, typePair, args);

		// we need to keep calling this... essentially looping.
		llvm::BasicBlock* curbb = cgi->builder.GetInsertBlock();	// store the current bb
		llvm::BasicBlock* loopBegin = llvm::BasicBlock::Create(cgi->getContext(), "loopBegin", curbb->getParent());
		llvm::BasicBlock* loopEnd = llvm::BasicBlock::Create(cgi->getContext(), "loopEnd", curbb->getParent());
		llvm::BasicBlock* after = llvm::BasicBlock::Create(cgi->getContext(), "afterLoop", curbb->getParent());



		// check for zero.
		if(isZero)
		{
			llvm::BasicBlock* notZero = llvm::BasicBlock::Create(cgi->getContext(), "notZero", curbb->getParent());
			llvm::BasicBlock* setToZero = llvm::BasicBlock::Create(cgi->getContext(), "zeroAlloc", curbb->getParent());
			cgi->builder.CreateCondBr(isZero, setToZero, notZero);

			cgi->builder.SetInsertPoint(setToZero);
			cgi->builder.CreateStore(llvm::ConstantPointerNull::getNullValue(allocatedmem->getType()), allocmemptr);
			allocatedmem = cgi->builder.CreateLoad(allocmemptr);
			cgi->builder.CreateBr(after);

			cgi->builder.SetInsertPoint(notZero);
		}









		// create the loop counter (initialise it with the value)
		llvm::Value* counterptr = cgi->allocateInstanceInBlock(allocsize->getType());
		cgi->builder.CreateStore(allocnum, counterptr);

		// do { ...; num--; } while(num - 1 > 0)
		cgi->builder.CreateBr(loopBegin);	// explicit branch


		// start in the loop
		cgi->builder.SetInsertPoint(loopBegin);

		// call the constructor
		allocatedmem = cgi->builder.CreateLoad(allocmemptr);
		args[0] = allocatedmem;
		cgi->builder.CreateCall(initfunc, args);

		// move the allocatedmem pointer by the type size
		cgi->doPointerArithmetic(ArithmeticOp::Add, allocatedmem, allocmemptr, oneValue);
		allocatedmem = cgi->builder.CreateLoad(allocmemptr);

		// subtract the counter
		llvm::Value* counter = cgi->builder.CreateLoad(counterptr);
		cgi->builder.CreateStore(cgi->builder.CreateSub(counter, oneValue), counterptr);

		// do the comparison
		counter = cgi->builder.CreateLoad(counterptr);

		llvm::Value* brcond = cgi->builder.CreateICmpUGT(counter, zeroValue);
		cgi->builder.CreateCondBr(brcond, loopBegin, loopEnd);

		// at loopend:
		cgi->builder.SetInsertPoint(loopEnd);

		// undo the pointer additions we did above
		cgi->doPointerArithmetic(ArithmeticOp::Subtract, allocatedmem, allocmemptr, allocnum);

		allocatedmem = cgi->builder.CreateLoad(allocmemptr);

		cgi->doPointerArithmetic(ArithmeticOp::Add, allocatedmem, allocmemptr, oneValue);


		cgi->builder.CreateBr(after);
		cgi->builder.SetInsertPoint(after);
		allocatedmem = cgi->builder.CreateLoad(allocmemptr);
	}

	return Result_t(allocatedmem, 0);
}


Result_t Dealloc::codegen(CodegenInstance* cgi, llvm::Value* lhsPtr, llvm::Value* rhs)
{
	llvm::Value* freearg = 0;
	if(dynamic_cast<VarRef*>(this->expr))
	{
		SymbolPair_t* sp = cgi->getSymPair(this, dynamic_cast<VarRef*>(this->expr)->name);
		if(!sp)
			error(cgi, this, "Unknown symbol '%s'", dynamic_cast<VarRef*>(this->expr)->name.c_str());

		sp->first.second = SymbolValidity::UseAfterDealloc;


		// this will be an alloca instance (aka pointer to whatever type it actually was)
		llvm::Value* varval = sp->first.first;

		// therefore, create a Load to get the actual value
		varval = cgi->builder.CreateLoad(varval);
		freearg = cgi->builder.CreatePointerCast(varval, llvm::IntegerType::getInt8PtrTy(cgi->getContext()));
	}
	else
	{
		freearg = this->expr->codegen(cgi).result.first;
	}

	// call 'free'
	FuncPair_t* fp = cgi->getOrDeclareLibCFunc(FREE_FUNC);
	cgi->builder.CreateCall(fp->first, freearg);
	return Result_t(0, 0);
}





















