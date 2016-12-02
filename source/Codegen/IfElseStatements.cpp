// ControlFlowCodegen.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "ast.h"
#include "codegen.h"

using namespace Ast;
using namespace Codegen;



static void codeGenRecursiveIf(CodegenInstance* cgi, fir::Function* func, std::deque<std::pair<Expr*, BracedBlock*>> pairs,
	fir::IRBlock* merge, fir::PHINode* phi, bool* didCreateMerge)
{
	if(pairs.size() == 0)
		return;

	fir::IRBlock* t = cgi->irb.addNewBlockInFunction("trueCaseR", func);
	fir::IRBlock* f = cgi->irb.addNewBlockInFunction("falseCaseR", func);

	fir::Value* cond = pairs.front().first->codegen(cgi).value;
	if(cond->getType() != fir::Type::getBool())
	{
		error(pairs.front().first, "Non-boolean type '%s' cannot be used as the conditional for an if statement",
			cond->getType()->str().c_str());
	}

	cond = cgi->irb.CreateICmpNEQ(cond, fir::ConstantValue::getNullValue(cond->getType()));


	cgi->irb.CreateCondBranch(cond, t, f);
	cgi->irb.setCurrentBlock(t);

	Result_t blockResult(0, 0);
	fir::Value* val = nullptr;
	{
		cgi->pushScope();
		blockResult = pairs.front().second->codegen(cgi);
		val = blockResult.value;
		cgi->popScope();
	}

	if(phi)
		phi->addIncoming(val, t);

	// check if the last expr of the block is a return
	if(blockResult.type != ResultType::BreakCodegen)
		cgi->irb.CreateUnCondBranch(merge), *didCreateMerge = true;

	// now the false case...
	// set the insert point to the false case, then go again.
	cgi->irb.setCurrentBlock(f);

	// recursively call ourselves
	pairs.pop_front();
	codeGenRecursiveIf(cgi, func, pairs, merge, phi, didCreateMerge);
}

Result_t IfStmt::codegen(CodegenInstance* cgi, fir::Value* extra)
{
	iceAssert(this->cases.size() > 0);

	fir::Value* firstCond = this->cases[0].first->codegen(cgi).value;
	if(firstCond->getType() != fir::Type::getBool())
	{
		error(this->cases[0].first, "Non-boolean type '%s' cannot be used as the conditional for an if statement",
			firstCond->getType()->str().c_str());
	}

	firstCond = cgi->irb.CreateICmpNEQ(firstCond, fir::ConstantValue::getNullValue(firstCond->getType()));


	fir::Function* func = cgi->irb.getCurrentBlock()->getParentFunction();
	iceAssert(func);

	fir::IRBlock* trueb = cgi->irb.addNewBlockInFunction("trueCase", func);
	fir::IRBlock* falseb = cgi->irb.addNewBlockInFunction("falseCase", func);
	fir::IRBlock* merge = cgi->irb.addNewBlockInFunction("merge", func);

	// create the first conditional
	cgi->irb.CreateCondBranch(firstCond, trueb, falseb);


	bool didMerge = false;

	// emit code for the first block
	fir::Value* truev = nullptr;
	{
		cgi->irb.setCurrentBlock(trueb);

		// push a new symtab
		cgi->pushScope();

		// generate the statements inside

		ResultType rt;
		std::tie(truev, rt) = this->cases[0].second->codegen(cgi);
		cgi->popScope();

		if(rt != ResultType::BreakCodegen)
			cgi->irb.CreateUnCondBranch(merge), didMerge = true;
	}



	// now for the clusterfuck.
	// to support if-elseif-elseif-elseif-...-else, we need to essentially compound/cascade conditionals in the 'else' block
	// of the if statement.

	cgi->irb.setCurrentBlock(falseb);

	// auto c1 = this->cases.front();
	this->cases.pop_front();

	fir::IRBlock* curblk = cgi->irb.getCurrentBlock();
	cgi->irb.setCurrentBlock(merge);

	// fir::PHINode* phi = builder.CreatePHI(fir::Type::getVoidTy(getContext()), this->cases.size() + (this->final ? 1 : 0));

	fir::PHINode* phi = nullptr;

	if(phi)
		phi->addIncoming(truev, trueb);

	cgi->irb.setCurrentBlock(curblk);
	codeGenRecursiveIf(cgi, func, std::deque<std::pair<Expr*, BracedBlock*>>(this->cases), merge, phi, &didMerge);


	// if we have an 'else' case
	ResultType elsetype = ResultType::Normal;
	if(this->final)
	{
		cgi->pushScope();

		fir::Value* v = 0;
		fir::Value* _ = 0;

		std::tie(v, _, elsetype) = this->final->codegen(cgi);

		cgi->popScope();

		if(phi) phi->addIncoming(v, falseb);
	}


	if(!this->final || elsetype != ResultType::BreakCodegen)
		cgi->irb.CreateUnCondBranch(merge), didMerge = true;


	if(didMerge)
	{
		cgi->irb.setCurrentBlock(merge);
	}
	else
	{
		merge->eraseFromParentFunction();
	}

	// restore.
	this->cases = this->_cases;
	return Result_t(0, 0);
}

fir::Type* IfStmt::getType(CodegenInstance* cgi, bool allowFail, fir::Value* extra)
{
	return 0;
}










