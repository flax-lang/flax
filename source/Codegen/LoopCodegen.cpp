// LoopCodegen.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "../include/ast.h"
#include "../include/codegen.h"
#include "../include/llvm_all.h"

using namespace Ast;
using namespace Codegen;


Result_t Break::codegen(CodegenInstance* cgi, llvm::Value* lhsPtr, llvm::Value* rhs)
{
	BracedBlockScope* cs = cgi->getCurrentBracedBlockScope();
	if(!cs)
		error(this, "Break can only be used inside loop bodies");

	assert(cs->first);
	assert(cs->second.first);
	assert(cs->second.second);

	// for break, we go to the ending block
	cgi->mainBuilder.CreateBr(cs->second.second);
	return Result_t(0, 0, ResultType::BreakCodegen);
}

Result_t Continue::codegen(CodegenInstance* cgi, llvm::Value* lhsPtr, llvm::Value* rhs)
{
	BracedBlockScope* cs = cgi->getCurrentBracedBlockScope();
	if(!cs)
		error(this, "Continue can only be used inside loop bodies");

	assert(cs->first);
	assert(cs->second.first);
	assert(cs->second.second);

	// for continue, we go to the beginning (loop) block
	cgi->mainBuilder.CreateBr(cs->second.first);
	return Result_t(0, 0, ResultType::BreakCodegen);
}

Result_t Return::codegen(CodegenInstance* cgi, llvm::Value* lhsPtr, llvm::Value* rhs)
{
	if(this->val)
	{
		auto res = this->val->codegen(cgi).result;
		llvm::Value* left = res.first;

		auto f = cgi->mainBuilder.GetInsertBlock()->getParent();
		assert(f);

		if(left->getType()->isIntegerTy() && f->getReturnType()->isIntegerTy())
			left = cgi->mainBuilder.CreateIntCast(left, f->getReturnType(), false);

		this->actualReturnValue = left;

		return Result_t(cgi->mainBuilder.CreateRet(left), res.second, ResultType::BreakCodegen);
	}
	else
	{
		return Result_t(cgi->mainBuilder.CreateRetVoid(), 0, ResultType::BreakCodegen);
	}
}

Result_t WhileLoop::codegen(CodegenInstance* cgi, llvm::Value* lhsPtr, llvm::Value* rhs)
{
	llvm::Function* parentFunc = cgi->mainBuilder.GetInsertBlock()->getParent();
	assert(parentFunc);

	llvm::BasicBlock* setupBlock = llvm::BasicBlock::Create(cgi->getContext(), "loopSetup", parentFunc);
	llvm::BasicBlock* loopBody = llvm::BasicBlock::Create(cgi->getContext(), "loopBody", parentFunc);
	llvm::BasicBlock* loopEnd = llvm::BasicBlock::Create(cgi->getContext(), "loopEnd", parentFunc);

	cgi->mainBuilder.CreateBr(setupBlock);
	cgi->mainBuilder.SetInsertPoint(setupBlock);

	llvm::Value* condOutside = this->cond->codegen(cgi).result.first;

	// branch to the body, since llvm doesn't allow unforced fallthroughs
	// if we're a do-while, don't check the condition the first time
	// else we should
	if(this->isDoWhileVariant)
		cgi->mainBuilder.CreateBr(loopBody);

	else
		cgi->mainBuilder.CreateCondBr(condOutside, loopBody, loopEnd);


	cgi->mainBuilder.SetInsertPoint(loopBody);
	cgi->pushBracedBlock(this, loopBody, loopEnd);

	this->body->codegen(cgi);

	cgi->popBracedBlock();

	// put a branch to see if we will go back
	llvm::Value* condInside = this->cond->codegen(cgi).result.first;
	cgi->mainBuilder.CreateCondBr(condInside, loopBody, loopEnd);


	// parentFunc->getBasicBlockList().push_back(loopEnd);
	cgi->mainBuilder.SetInsertPoint(loopEnd);

	return Result_t(0, 0);
}


