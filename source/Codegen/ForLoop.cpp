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
	if(this->init) this->init->codegen(cgi);
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
	error(this, "Cannot get type of typeless construct (for loop)");
}









// for _ in ...

static void codegenForRange(CodegenInstance* cgi, ForInLoop* fl, fir::Value* range)
{
	VarRef* vr = dynamic_cast<VarRef*>(fl->var);
	if(!vr)
		error(fl->var, "For-in loops on ranges only support single variables (ie. expected identifier)");

	// kappa
	auto curFn = cgi->irb.getCurrentFunction();
	auto loopSetup = cgi->irb.addNewBlockInFunction("loopSetup", curFn);
	auto loopCond = cgi->irb.addNewBlockInFunction("loopCond", curFn);
	auto loopBody = cgi->irb.addNewBlockInFunction("loopBody", curFn);
	auto merge = cgi->irb.addNewBlockInFunction("merge", curFn);

	cgi->irb.CreateUnCondBranch(loopSetup);
	cgi->irb.setCurrentBlock(loopSetup);

	fir::Value* lowerbd = cgi->irb.CreateGetRangeLower(range);
	fir::Value* upperbd = cgi->irb.CreateGetRangeUpper(range);

	cgi->pushScope();
	cgi->pushBracedBlock(fl, loopBody, merge);

	fir::Value* counter = cgi->getStackAlloc(fir::Type::getInt64());
	VarDecl* fakedecl = new VarDecl(vr->pin, vr->name, false);
	fakedecl->concretisedType = counter->getType()->getPointerElementType();

	cgi->addSymbol(vr->name, counter, fakedecl);

	cgi->irb.CreateStore(lowerbd, counter);

	// ok, setup done
	cgi->irb.CreateUnCondBranch(loopCond);
	cgi->irb.setCurrentBlock(loopCond);
	{
		// if the counter becomes > the uppperbound, we know to quit.
		// the range itself handles the half-open thing, so upperbd is always inclusive for us.
		fir::Value* cond = cgi->irb.CreateICmpGT(cgi->irb.CreateLoad(counter), upperbd);

		// since the condition was ctr > max, we 'flip' the thing (true = quit, false = continue)
		cgi->irb.CreateCondBranch(cond, merge, loopBody);
	}

	cgi->irb.setCurrentBlock(loopBody);
	{
		fl->body->codegen(cgi);

		// increment counter
		cgi->irb.CreateStore(cgi->irb.CreateAdd(cgi->irb.CreateLoad(counter), fir::ConstantInt::getInt64(1)), counter);
		cgi->irb.CreateUnCondBranch(loopCond);
	}

	cgi->popBracedBlock();
	cgi->popScope();

	cgi->irb.setCurrentBlock(merge);

	// ok, done
}

Result_t ForInLoop::codegen(CodegenInstance* cgi, fir::Type* extratype, fir::Value* target)
{
	// check what is the right side
	fir::Value* rhs = 0; fir::Value* rhsptr = 0; ValueKind vk;
	std::tie(rhs, rhsptr, vk) = this->rhs->codegen(cgi);

	if(rhs->getType()->isRangeType())
	{
		// this is basically a counter-ish thing
		// do a load+store/counter kind of deal

		codegenForRange(cgi, this, rhs);
	}
	else if(rhs->getType()->isArrayType())
	{
	}
	else if(rhs->getType()->isDynamicArrayType())
	{
	}
	else if(rhs->getType()->isArraySliceType())
	{
	}
	else if(rhs->getType()->isStringType())
	{
	}
	else
	{
	}

	return Result_t(0, 0);
}

fir::Type* ForInLoop::getType(CodegenInstance* cgi, fir::Type* extratype, bool allowFail)
{
	error(this, "Cannot get type of typeless construct (for-in loop)");
}




