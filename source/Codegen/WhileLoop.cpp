// WhileLoop.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "codegen.h"

using namespace Ast;
using namespace Codegen;


Result_t WhileLoop::codegen(CodegenInstance* cgi, fir::Type* extratype, fir::Value* target)
{
	fir::Function* parentFunc = cgi->irb.getCurrentBlock()->getParentFunction();
	iceAssert(parentFunc);

	fir::IRBlock* setupBlock = cgi->irb.addNewBlockInFunction("loopSetup", parentFunc);
	fir::IRBlock* loopBody = cgi->irb.addNewBlockInFunction("loopBody", parentFunc);
	fir::IRBlock* loopEnd = cgi->irb.addNewBlockInFunction("loopEnd", parentFunc);

	cgi->irb.CreateUnCondBranch(setupBlock);
	cgi->irb.setCurrentBlock(setupBlock);

	fir::Value* condOutside = this->cond->codegen(cgi).value;

	// branch to the body, since we don't allow unforced fallthroughs in the ir
	// if we're a do-while, don't check the condition the first time
	// else we should
	if(this->isDoWhileVariant)
		cgi->irb.CreateUnCondBranch(loopBody);

	else
		cgi->irb.CreateCondBranch(condOutside, loopBody, loopEnd);


	cgi->irb.setCurrentBlock(loopBody);
	cgi->pushBracedBlock(this, loopBody, loopEnd);

	auto res = this->body->codegen(cgi);

	cgi->popBracedBlock();

	// see if we broke codegen
	if(res.type != ResultType::BreakCodegen)
	{
		// put a branch to see if we will go back
		fir::Value* condInside = this->cond->codegen(cgi).value;
		cgi->irb.CreateCondBranch(condInside, loopBody, loopEnd);
	}

	// parentFunc->getBasicBlockList().push_back(loopEnd);
	cgi->irb.setCurrentBlock(loopEnd);

	return Result_t(0, 0);
}

fir::Type* WhileLoop::getType(CodegenInstance* cgi, fir::Type* extratype, bool allowFail)
{
	return 0;
}






