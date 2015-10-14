// LoopCodegen.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "../include/ast.h"
#include "../include/codegen.h"

#include "llvm/IR/Function.h"

using namespace Ast;
using namespace Codegen;


Result_t Break::codegen(CodegenInstance* cgi, fir::Value* lhsPtr, fir::Value* rhs)
{
	BracedBlockScope* cs = cgi->getCurrentBracedBlockScope();
	if(!cs)
	{
		error(this, "Break can only be used inside loop bodies");
	}

	iceAssert(cs->first);
	iceAssert(cs->second.first);
	iceAssert(cs->second.second);

	// for break, we go to the ending block
	cgi->builder.CreateBr(cs->second.second);
	return Result_t(0, 0, ResultType::BreakCodegen);
}

Result_t Continue::codegen(CodegenInstance* cgi, fir::Value* lhsPtr, fir::Value* rhs)
{
	BracedBlockScope* cs = cgi->getCurrentBracedBlockScope();
	if(!cs)
	{
		error(this, "Continue can only be used inside loop bodies");
	}

	iceAssert(cs->first);
	iceAssert(cs->second.first);
	iceAssert(cs->second.second);

	// for continue, we go to the beginning (loop) block
	cgi->builder.CreateBr(cs->second.first);
	return Result_t(0, 0, ResultType::BreakCodegen);
}

Result_t Return::codegen(CodegenInstance* cgi, fir::Value* lhsPtr, fir::Value* rhs)
{
	if(this->val)
	{
		auto res = this->val->codegen(cgi).result;
		fir::Value* left = res.first;

		auto f = cgi->builder.GetInsertBlock()->getParent();
		iceAssert(f);

		if(left->getType()->isIntegerTy() && f->getReturnType()->isIntegerTy())
			left = cgi->builder.CreateIntCast(left, f->getReturnType(), false);

		this->actualReturnValue = left;

		return Result_t(cgi->builder.CreateRet(left), res.second, ResultType::BreakCodegen);
	}
	else
	{
		return Result_t(cgi->builder.CreateRetVoid(), 0, ResultType::BreakCodegen);
	}
}


Result_t DeferredExpr::codegen(CodegenInstance* cgi, fir::Value* lhsPtr, fir::Value* rhs)
{
	return expr->codegen(cgi);
}

Result_t WhileLoop::codegen(CodegenInstance* cgi, fir::Value* lhsPtr, fir::Value* rhs)
{
	fir::Function* parentFunc = cgi->builder.GetInsertBlock()->getParent();
	iceAssert(parentFunc);

	fir::BasicBlock* setupBlock = fir::BasicBlock::Create(cgi->getContext(), "loopSetup", parentFunc);
	fir::BasicBlock* loopBody = fir::BasicBlock::Create(cgi->getContext(), "loopBody", parentFunc);
	fir::BasicBlock* loopEnd = fir::BasicBlock::Create(cgi->getContext(), "loopEnd", parentFunc);

	cgi->builder.CreateBr(setupBlock);
	cgi->builder.SetInsertPoint(setupBlock);

	fir::Value* condOutside = this->cond->codegen(cgi).result.first;

	// branch to the body, since llvm doesn't allow unforced fallthroughs
	// if we're a do-while, don't check the condition the first time
	// else we should
	if(this->isDoWhileVariant)
		cgi->builder.CreateBr(loopBody);

	else
		cgi->builder.CreateCondBr(condOutside, loopBody, loopEnd);


	cgi->builder.SetInsertPoint(loopBody);
	cgi->pushBracedBlock(this, loopBody, loopEnd);

	this->body->codegen(cgi);

	cgi->popBracedBlock();

	// put a branch to see if we will go back
	fir::Value* condInside = this->cond->codegen(cgi).result.first;
	cgi->builder.CreateCondBr(condInside, loopBody, loopEnd);


	// parentFunc->getBasicBlockList().push_back(loopEnd);
	cgi->builder.SetInsertPoint(loopEnd);

	return Result_t(0, 0);
}


