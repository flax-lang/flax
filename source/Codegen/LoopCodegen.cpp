// LoopCodegen.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "../include/ast.h"
#include "../include/codegen.h"
#include "../include/llvm_all.h"

using namespace Ast;
using namespace Codegen;

ValPtr_p WhileLoop::codegen(CodegenInstance* cgi)
{
	llvm::Function* parentFunc = cgi->mainBuilder.GetInsertBlock()->getParent();
	llvm::BasicBlock* setupBlock = llvm::BasicBlock::Create(cgi->getContext(), "loopSetup", parentFunc);
	llvm::BasicBlock* loopBody = llvm::BasicBlock::Create(cgi->getContext(), "loopBody", parentFunc);
	llvm::BasicBlock* loopEnd = llvm::BasicBlock::Create(cgi->getContext(), "loopEnd");

	cgi->mainBuilder.CreateBr(setupBlock);
	cgi->mainBuilder.SetInsertPoint(setupBlock);

	llvm::Value* condOutside = this->cond->codegen(cgi).first;

	// branch to the body, since llvm doesn't allow unforced fallthroughs
	// if we're a do-while, don't check the condition the first time
	// else we should
	if(this->isDoWhileVariant)
		cgi->mainBuilder.CreateBr(loopBody);

	else
		cgi->mainBuilder.CreateCondBr(condOutside, loopBody, loopEnd);


	cgi->mainBuilder.SetInsertPoint(loopBody);

	this->body->codegen(cgi);

	// put a branch to see if we will go back
	llvm::Value* condInside = this->cond->codegen(cgi).first;
	cgi->mainBuilder.CreateCondBr(condInside, loopBody, loopEnd);


	parentFunc->getBasicBlockList().push_back(loopEnd);
	cgi->mainBuilder.SetInsertPoint(loopEnd);

	return ValPtr_p(0, 0);
}
