// AllocCodegen.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "codegen.h"

using namespace Ast;
using namespace Codegen;


#define MALLOC_FUNC		"malloc"
#define FREE_FUNC		"free"

static fir::Value* recursivelyDoAlloc(CodegenInstance* cgi, fir::Type* type, fir::Value* size, std::deque<Expr*> params,
	std::deque<fir::Value*>& sizes)
{
	fir::Function* mallocf = cgi->getOrDeclareLibCFunc(MALLOC_FUNC);
	iceAssert(mallocf);

	mallocf = cgi->module->getFunction(mallocf->getName());
	iceAssert(mallocf);


	fir::Value* oneValue = fir::ConstantInt::getInt64(1, cgi->getContext());
	fir::Value* zeroValue = fir::ConstantInt::getInt64(0, cgi->getContext());

	uint64_t typesize = cgi->execTarget->getTypeSizeInBits(type) / 8;
	fir::Value* allocsize = fir::ConstantInt::getInt64(typesize, cgi->getContext());

	size = cgi->autoCastType(allocsize->getType(), size);

	fir::Value* totalAlloc = cgi->irb.CreateMul(allocsize, size, "totalalloc");
	fir::Value* allocmemptr = cgi->getStackAlloc(type->getPointerTo(), "allocmemptr");

	fir::Value* amem = cgi->irb.CreatePointerTypeCast(cgi->irb.CreateCall1(mallocf, totalAlloc), type->getPointerTo());
	cgi->irb.CreateStore(amem, allocmemptr);



	fir::IRBlock* curbb = cgi->irb.getCurrentBlock();	// store the current bb
	fir::IRBlock* loopHead = cgi->irb.addNewBlockInFunction("loopHead", curbb->getParentFunction());
	fir::IRBlock* loopBegin = cgi->irb.addNewBlockInFunction("loopBegin", curbb->getParentFunction());
	fir::IRBlock* loopEnd = cgi->irb.addNewBlockInFunction("loopEnd", curbb->getParentFunction());

	fir::IRBlock* allocZeroCase = cgi->irb.addNewBlockInFunction("allocZeroCase", curbb->getParentFunction());
	fir::IRBlock* loopMerge = cgi->irb.addNewBlockInFunction("loopMerge", curbb->getParentFunction());

	cgi->irb.setCurrentBlock(curbb);
	cgi->irb.CreateUnCondBranch(loopHead);

	cgi->irb.setCurrentBlock(loopHead);

	// set a counter
	fir::Value* counterPtr = cgi->irb.CreateStackAlloc(fir::Type::getInt64(), "counterPtr");
	cgi->irb.CreateStore(zeroValue, counterPtr);

	// check for zero.
	{
		fir::Value* isZero = cgi->irb.CreateICmpEQ(size, zeroValue, "iszero");
		cgi->irb.CreateCondBranch(isZero, allocZeroCase, loopBegin);
	}


	// begin the loop
	cgi->irb.setCurrentBlock(loopBegin);
	{
		// get the pointer.
		fir::Value* pointer = cgi->irb.CreateGetPointer(cgi->irb.CreateLoad(allocmemptr),
			cgi->irb.CreateLoad(counterPtr), "pointerPtr");

		if(type->isStructType() || type->isClassType())
		{
			// call the init func
			TypePair_t* typePair = 0;

			std::vector<fir::Value*> args;
			args.push_back(pointer);
			for(Expr* e : params)
				args.push_back(e->codegen(cgi).value);

			typePair = cgi->getType(type);
			fir::Function* initfunc = cgi->getStructInitialiser(/* user */ 0, typePair, args);
			iceAssert(initfunc);

			cgi->irb.CreateCall(initfunc, args);
		}
		else if(type->isPointerType() && sizes.size() > 0)
		{
			fir::Value* front = sizes.front();
			sizes.pop_front();

			fir::Value* rret = recursivelyDoAlloc(cgi, type->getPointerElementType(), front, params, sizes);
			cgi->irb.CreateStore(rret, pointer);
		}
		else
		{
			cgi->irb.CreateStore(fir::ConstantValue::getNullValue(type), pointer);
		}


		// increment counter
		fir::Value* incremented = cgi->irb.CreateAdd(oneValue, cgi->irb.CreateLoad(counterPtr));
		cgi->irb.CreateStore(incremented, counterPtr);


		// check.
		// basically this: if counter < size goto loopBegin else goto loopEnd

		cgi->irb.CreateCondBranch(cgi->irb.CreateICmpLT(cgi->irb.CreateLoad(counterPtr), size), loopBegin, loopEnd);
	}


	// in zeroBlock, set the pointer to 0 and branch to merge
	cgi->irb.setCurrentBlock(allocZeroCase);
	{
		cgi->irb.CreateStore(fir::ConstantValue::getNullValue(type->getPointerTo()), allocmemptr);
		cgi->irb.CreateUnCondBranch(loopMerge);
	}



	// in end block... do nothing.
	cgi->irb.setCurrentBlock(loopEnd);
	{
		cgi->irb.CreateUnCondBranch(loopMerge);
	}


	cgi->irb.setCurrentBlock(loopMerge);
	fir::Value* ret = cgi->irb.CreateLoad(allocmemptr, "mem");

	return ret;
}























Result_t Alloc::codegen(CodegenInstance* cgi, fir::Value* extra)
{
	// if we haven't declared malloc() yet, then we need to do it here
	// NOTE: this is the only place in the compiler where a hardcoded call is made to a non-provided function.

	fir::Type* allocType = 0;

	allocType = cgi->getTypeFromParserType(this, this->ptype);
	iceAssert(allocType);




	// call malloc


	fir::Value* oneValue = fir::ConstantInt::getInt64(1, cgi->getContext());
	// fir::Value* zeroValue = fir::ConstantInt::getUint64(0, cgi->getContext());


	if(this->counts.size() > 0)
	{
		std::deque<fir::Value*> cnts;

		for(auto c : this->counts)
			cnts.push_back(c->codegen(cgi).value);

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

fir::Type* Alloc::getType(CodegenInstance* cgi, bool allowFail, fir::Value* extra)
{
	fir::Type* base = cgi->getTypeFromParserType(this, this->ptype);

	if(this->counts.size() == 0) return base->getPointerTo();

	for(size_t i = 0; i < this->counts.size(); i++)
		 base = base->getPointerTo();

	return base;
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
		varval = cgi->irb.CreateLoad(varval);
		freearg = cgi->irb.CreatePointerTypeCast(varval, fir::Type::getInt8Ptr(cgi->getContext()));
	}
	else
	{
		freearg = this->expr->codegen(cgi).value;
	}

	// call 'free'
	fir::Function* freef = cgi->getOrDeclareLibCFunc(FREE_FUNC);
	iceAssert(freef);

	freef = cgi->module->getFunction(freef->getName());
	iceAssert(freef);


	cgi->irb.CreateCall1(freef, freearg);
	return Result_t(0, 0);
}

fir::Type* Dealloc::getType(CodegenInstance* cgi, bool allowFail, fir::Value* extra)
{
	return 0;
}




















