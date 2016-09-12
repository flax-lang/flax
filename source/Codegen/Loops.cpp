// LoopCodegen.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "codegen.h"

using namespace Ast;
using namespace Codegen;


Result_t Break::codegen(CodegenInstance* cgi, fir::Value* extra)
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
	cgi->builder.CreateUnCondBranch(cs->second.second);
	return Result_t(0, 0, ResultType::BreakCodegen);
}

fir::Type* Break::getType(CodegenInstance* cgi, bool allowFail, fir::Value* extra)
{
	iceAssert(0);
}







Result_t Continue::codegen(CodegenInstance* cgi, fir::Value* extra)
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
	cgi->builder.CreateUnCondBranch(cs->second.first);
	return Result_t(0, 0, ResultType::BreakCodegen);
}

fir::Type* Continue::getType(CodegenInstance* cgi, bool allowFail, fir::Value* extra)
{
	iceAssert(0);
}




Result_t Return::codegen(CodegenInstance* cgi, fir::Value* extra)
{
	if(this->val)
	{
		auto res = this->val->codegen(cgi).result;
		fir::Value* left = res.first;

		fir::Function* f = cgi->builder.getCurrentBlock()->getParentFunction();
		iceAssert(f);

		this->actualReturnValue = cgi->autoCastType(f->getReturnType(), left, res.second);

		return Result_t(cgi->builder.CreateReturn(this->actualReturnValue), res.second, ResultType::BreakCodegen);
	}
	else
	{
		return Result_t(cgi->builder.CreateReturnVoid(), 0, ResultType::BreakCodegen);
	}
}

fir::Type* Return::getType(CodegenInstance* cgi, bool allowFail, fir::Value* extra)
{
	if(this->val) return this->val->getType(cgi);
	else return fir::PrimitiveType::getVoid();
}



Result_t DeferredExpr::codegen(CodegenInstance* cgi, fir::Value* extra)
{
	return expr->codegen(cgi);
}

fir::Type* DeferredExpr::getType(CodegenInstance* cgi, bool allowFail, fir::Value* extra)
{
	return this->expr->getType(cgi);
}







Result_t WhileLoop::codegen(CodegenInstance* cgi, fir::Value* extra)
{
	fir::Function* parentFunc = cgi->builder.getCurrentBlock()->getParentFunction();
	iceAssert(parentFunc);

	fir::IRBlock* setupBlock = cgi->builder.addNewBlockInFunction("loopSetup", parentFunc);
	fir::IRBlock* loopBody = cgi->builder.addNewBlockInFunction("loopBody", parentFunc);
	fir::IRBlock* loopEnd = cgi->builder.addNewBlockInFunction("loopEnd", parentFunc);

	cgi->builder.CreateUnCondBranch(setupBlock);
	cgi->builder.setCurrentBlock(setupBlock);

	fir::Value* condOutside = this->cond->codegen(cgi).result.first;

	// branch to the body, since we don't allow unforced fallthroughs in the ir
	// if we're a do-while, don't check the condition the first time
	// else we should
	if(this->isDoWhileVariant)
		cgi->builder.CreateUnCondBranch(loopBody);

	else
		cgi->builder.CreateCondBranch(condOutside, loopBody, loopEnd);


	cgi->builder.setCurrentBlock(loopBody);
	cgi->pushBracedBlock(this, loopBody, loopEnd);

	this->body->codegen(cgi);

	cgi->popBracedBlock();

	// put a branch to see if we will go back
	fir::Value* condInside = this->cond->codegen(cgi).result.first;
	cgi->builder.CreateCondBranch(condInside, loopBody, loopEnd);


	// parentFunc->getBasicBlockList().push_back(loopEnd);
	cgi->builder.setCurrentBlock(loopEnd);

	return Result_t(0, 0);
}

fir::Type* WhileLoop::getType(CodegenInstance* cgi, bool allowFail, fir::Value* extra)
{
	return 0;
}






