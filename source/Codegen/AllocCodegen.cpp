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

	FuncPair_t* fp = cgi->getDeclaredFunc(MALLOC_FUNC);
	if(!fp)
	{
		VarDecl* fakefdmvd = new VarDecl(this->posinfo, "size", false);
		fakefdmvd->type = "Uint64";

		std::deque<VarDecl*> params;
		params.push_back(fakefdmvd);
		FuncDecl* fakefm = new FuncDecl(this->posinfo, MALLOC_FUNC, params, "Int8*");
		fakefm->isFFI = true;

		fakefm->codegen(cgi);

		assert((fp = cgi->getDeclaredFunc(MALLOC_FUNC)));
	}

	llvm::Function* mallocf = fp->first;
	assert(mallocf);

	llvm::Type* allocType = 0;
	TypePair_t* typePair = 0;

	allocType = cgi->getLlvmType(this, this->type);
	assert(allocType);


	llvm::Value* oneValue = llvm::ConstantInt::get(llvm::IntegerType::getInt64Ty(cgi->getContext()), 1);
	llvm::Value* zeroValue = llvm::ConstantInt::get(llvm::IntegerType::getInt64Ty(cgi->getContext()), 0);



	// call malloc
	uint64_t typesize = cgi->mainModule->getDataLayout()->getTypeSizeInBits(allocType) / 8;
	llvm::Value* allocsize = llvm::ConstantInt::get(llvm::IntegerType::getInt64Ty(cgi->getContext()), typesize);
	llvm::Value* allocnum = oneValue;


	llvm::Value* isZero = nullptr;
	if(this->count)
	{
		allocnum = this->count->codegen(cgi).result.first;
		if(!allocnum->getType()->isIntegerTy())
			error(this, "Expected integer type in alloc");

		allocnum = cgi->mainBuilder.CreateIntCast(allocnum, allocsize->getType(), false);
		allocsize = cgi->mainBuilder.CreateMul(allocsize, allocnum);


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
			isZero = cgi->mainBuilder.CreateICmpEQ(allocnum, llvm::ConstantInt::getNullValue(allocsize->getType()));
		}
	}

	llvm::Value* allocmemptr = lhsPtr ? lhsPtr : cgi->allocateInstanceInBlock(allocType->getPointerTo());
	llvm::Value* allocatedmem = cgi->mainBuilder.CreateStore(cgi->mainBuilder.CreatePointerCast(cgi->mainBuilder.CreateCall(mallocf, allocsize), allocType->getPointerTo()), allocmemptr);

	allocatedmem = cgi->mainBuilder.CreateLoad(allocmemptr);

	// call the initialiser, if there is one
	llvm::Value* defaultValue = 0;
	if(allocType->isIntegerTy() || allocType->isPointerTy())
	{
		defaultValue = llvm::Constant::getNullValue(allocType);
		cgi->mainBuilder.CreateMemSet(allocatedmem, defaultValue, allocsize, typesize);
	}
	else
	{
		std::vector<llvm::Value*> args;
		args.push_back(allocatedmem);
		for(Expr* e : this->params)
			args.push_back(e->codegen(cgi).result.first);

		typePair = cgi->getType(allocType);

		llvm::Function* initfunc = cgi->getStructInitialiser(this, typePair, args);

		// we need to keep calling this... essentially looping.
		llvm::BasicBlock* curbb = cgi->mainBuilder.GetInsertBlock();	// store the current bb
		llvm::BasicBlock* loopBegin = llvm::BasicBlock::Create(cgi->getContext(), "loopBegin", curbb->getParent());
		llvm::BasicBlock* loopEnd = llvm::BasicBlock::Create(cgi->getContext(), "loopEnd", curbb->getParent());
		llvm::BasicBlock* after = llvm::BasicBlock::Create(cgi->getContext(), "afterLoop", curbb->getParent());



		// check for zero.
		if(isZero)
		{
			llvm::BasicBlock* notZero = llvm::BasicBlock::Create(cgi->getContext(), "notZero", curbb->getParent());
			llvm::BasicBlock* setToZero = llvm::BasicBlock::Create(cgi->getContext(), "zeroAlloc", curbb->getParent());
			cgi->mainBuilder.CreateCondBr(isZero, setToZero, notZero);

			cgi->mainBuilder.SetInsertPoint(setToZero);
			cgi->mainBuilder.CreateStore(llvm::ConstantPointerNull::getNullValue(allocatedmem->getType()), allocmemptr);
			allocatedmem = cgi->mainBuilder.CreateLoad(allocmemptr);
			cgi->mainBuilder.CreateBr(after);

			cgi->mainBuilder.SetInsertPoint(notZero);
		}









		// create the loop counter (initialise it with the value)
		llvm::Value* counterptr = cgi->allocateInstanceInBlock(allocsize->getType());
		cgi->mainBuilder.CreateStore(allocnum, counterptr);
		llvm::Value* counter = cgi->mainBuilder.CreateLoad(counterptr);

		// do { ...; num--; } while(num - 1 > 0)
		cgi->mainBuilder.CreateBr(loopBegin);	// explicit branch


		// start in the loop
		cgi->mainBuilder.SetInsertPoint(loopBegin);

		// call the constructor
		allocatedmem = cgi->mainBuilder.CreateLoad(allocmemptr);
		args[0] = allocatedmem;
		cgi->mainBuilder.CreateCall(initfunc, args);

		// move the allocatedmem pointer by the type size
		cgi->doPointerArithmetic(ArithmeticOp::Add, allocatedmem, allocmemptr, oneValue);
		allocatedmem = cgi->mainBuilder.CreateLoad(allocmemptr);

		// subtract the counter
		counter = cgi->mainBuilder.CreateLoad(counterptr);
		cgi->mainBuilder.CreateStore(cgi->mainBuilder.CreateSub(counter, oneValue), counterptr);

		// do the comparison
		counter = cgi->mainBuilder.CreateLoad(counterptr);

		llvm::Value* brcond = cgi->mainBuilder.CreateICmpUGT(counter, zeroValue);
		cgi->mainBuilder.CreateCondBr(brcond, loopBegin, loopEnd);

		// at loopend:
		cgi->mainBuilder.SetInsertPoint(loopEnd);

		// undo the pointer additions we did above
		cgi->doPointerArithmetic(ArithmeticOp::Subtract, allocatedmem, allocmemptr, allocnum);

		allocatedmem = cgi->mainBuilder.CreateLoad(allocmemptr);

		cgi->doPointerArithmetic(ArithmeticOp::Add, allocatedmem, allocmemptr, oneValue);


		cgi->mainBuilder.CreateBr(after);
		cgi->mainBuilder.SetInsertPoint(after);
		allocatedmem = cgi->mainBuilder.CreateLoad(allocmemptr);
	}

	return Result_t(allocatedmem, 0);
}


Result_t Dealloc::codegen(CodegenInstance* cgi, llvm::Value* lhsPtr, llvm::Value* rhs)
{
	SymbolPair_t* sp = cgi->getSymPair(this, this->var->name);
	if(!sp)
		error(this, "Unknown symbol '%s'", this->var->name.c_str());

	sp->first.second = SymbolValidity::UseAfterDealloc;

	// call 'free'
	FuncPair_t* fp = cgi->getDeclaredFunc(FREE_FUNC);
	if(!fp)
	{
		VarDecl* fakefdmvd = new VarDecl(this->posinfo, "ptr", false);
		fakefdmvd->type = "Int8*";

		std::deque<VarDecl*> params;
		params.push_back(fakefdmvd);
		FuncDecl* fakefm = new FuncDecl(this->posinfo, MALLOC_FUNC, params, "Int8*");
		fakefm->isFFI = true;

		fakefm->codegen(cgi);

		assert((fp = cgi->getDeclaredFunc(FREE_FUNC)));
	}

	// this will be an alloca instance (aka pointer to whatever type it actually was)
	llvm::Value* varval = sp->first.first;

	// therefore, create a Load to get the actual value
	varval = cgi->mainBuilder.CreateLoad(varval);
	llvm::Value* freearg = cgi->mainBuilder.CreatePointerCast(varval, llvm::IntegerType::getInt8PtrTy(cgi->getContext()));

	cgi->mainBuilder.CreateCall(fp->first, freearg);
	return Result_t(0, 0);
}





















