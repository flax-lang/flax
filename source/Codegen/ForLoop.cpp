// ForLoop.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "codegen.h"

using namespace Ast;
using namespace Codegen;


Result_t ForLoop::codegen(CodegenInstance* cgi, fir::Type* extratype, fir::Value* target)
{
	fir::Function* parentFunc = cgi->irb.getCurrentBlock()->getParentFunction();
	iceAssert(parentFunc);

	fir::IRBlock* initBlk = cgi->irb.addNewBlockInFunction("loopInit", parentFunc);
	fir::IRBlock* condBlk = cgi->irb.addNewBlockInFunction("loopCond", parentFunc);
	fir::IRBlock* bodyBlk = cgi->irb.addNewBlockInFunction("loopBody", parentFunc);
	fir::IRBlock* incrBlk = cgi->irb.addNewBlockInFunction("loopIncr", parentFunc);
	fir::IRBlock* merge = cgi->irb.addNewBlockInFunction("loopEnd", parentFunc);

	cgi->irb.CreateUnCondBranch(initBlk);
	cgi->irb.setCurrentBlock(initBlk);

	// make a new scope first
	cgi->pushScope();
	cgi->pushBracedBlock(this, incrBlk, merge);

	// generate init (shouldn't return a value)
	iceAssert(this->init);
	this->init->codegen(cgi);
	cgi->irb.CreateUnCondBranch(condBlk);


	cgi->irb.setCurrentBlock(condBlk);


	// check the condition
	iceAssert(this->cond);
	{
		fir::Value* c = this->cond->codegen(cgi).value;
		iceAssert(c);

		if(c->getType() != fir::Type::getBool())
			error(this->cond, "Expected bool type in for-loop condition, got '%s' instead", c->getType()->str().c_str());

		// do a conditional branch
		cgi->irb.CreateCondBranch(c, bodyBlk, merge);
	}


	// do the incr
	cgi->irb.setCurrentBlock(incrBlk);

	for(auto in : this->incrs)
		in->codegen(cgi);

	// branch
	cgi->irb.CreateUnCondBranch(condBlk);



	// ok, now the body
	cgi->irb.setCurrentBlock(bodyBlk);

	this->body->codegen(cgi);
	cgi->irb.CreateUnCondBranch(incrBlk);


	cgi->popBracedBlock();
	cgi->popScope();

	cgi->irb.setCurrentBlock(merge);

	return Result_t(0, 0);
}

fir::Type* ForLoop::getType(CodegenInstance* cgi, fir::Type* extratype, bool allowFail)
{
	return 0;
}






