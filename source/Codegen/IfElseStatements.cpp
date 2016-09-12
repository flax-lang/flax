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

	fir::IRBlock* t = cgi->builder.addNewBlockInFunction("trueCaseR", func);
	fir::IRBlock* f = new fir::IRBlock();
	f->setName("falseCaseR");
	f->setFunction(func);


	fir::Value* cond = pairs.front().first->codegen(cgi).result.first;
	cond = cgi->builder.CreateICmpNEQ(cond, fir::ConstantValue::getNullValue(cond->getType()));


	cgi->builder.CreateCondBranch(cond, t, f);
	cgi->builder.setCurrentBlock(t);

	Result_t blockResult(0, 0);
	fir::Value* val = nullptr;
	{
		cgi->pushScope();
		blockResult = pairs.front().second->codegen(cgi);
		val = blockResult.result.first;
		cgi->popScope();
	}

	if(phi)
		phi->addIncoming(val, t);

	// check if the last expr of the block is a return
	if(blockResult.type != ResultType::BreakCodegen)
		cgi->builder.CreateUnCondBranch(merge), *didCreateMerge = true;


	// now the false case...
	// set the insert point to the false case, then go again.
	cgi->builder.setCurrentBlock(f);

	// recursively call ourselves
	pairs.pop_front();
	codeGenRecursiveIf(cgi, func, pairs, merge, phi, didCreateMerge);

	// once that's done, we can add the false-case block to the func
	func->getBlockList().push_back(f);
}

Result_t IfStmt::codegen(CodegenInstance* cgi, fir::Value* extra)
{
	iceAssert(this->cases.size() > 0);

	fir::Value* firstCond = this->cases[0].first->codegen(cgi).result.first;
	firstCond = cgi->builder.CreateICmpNEQ(firstCond, fir::ConstantValue::getNullValue(firstCond->getType()));



	fir::Function* func = cgi->builder.getCurrentBlock()->getParentFunction();
	iceAssert(func);

	fir::IRBlock* trueb = cgi->builder.addNewBlockInFunction("trueCase", func);
	fir::IRBlock* falseb = cgi->builder.addNewBlockInFunction("falseCase", func);
	fir::IRBlock* merge = cgi->builder.addNewBlockInFunction("merge", func);

	// create the first conditional
	cgi->builder.CreateCondBranch(firstCond, trueb, falseb);


	bool didMerge = false;

	// emit code for the first block
	fir::Value* truev = nullptr;
	{
		cgi->builder.setCurrentBlock(trueb);

		// push a new symtab
		cgi->pushScope();

		// generate the statements inside
		Result_t cresult = this->cases[0].second->codegen(cgi);
		truev = cresult.result.first;
		cgi->popScope();

		if(cresult.type != ResultType::BreakCodegen)
			cgi->builder.CreateUnCondBranch(merge), didMerge = true;
	}



	// now for the clusterfuck.
	// to support if-elseif-elseif-elseif-...-else, we need to essentially compound/cascade conditionals in the 'else' block
	// of the if statement.

	cgi->builder.setCurrentBlock(falseb);

	auto c1 = this->cases.front();
	this->cases.pop_front();

	fir::IRBlock* curblk = cgi->builder.getCurrentBlock();
	cgi->builder.setCurrentBlock(merge);

	// fir::PHINode* phi = builder.CreatePHI(fir::Type::getVoidTy(getContext()), this->cases.size() + (this->final ? 1 : 0));

	fir::PHINode* phi = nullptr;

	if(phi)
		phi->addIncoming(truev, trueb);

	cgi->builder.setCurrentBlock(curblk);
	codeGenRecursiveIf(cgi, func, std::deque<std::pair<Expr*, BracedBlock*>>(this->cases), merge, phi, &didMerge);

	// func->getBasicBlockList().push_back(falseb);

	// if we have an 'else' case
	Result_t elseResult(0, 0);
	if(this->final)
	{
		cgi->pushScope();
		elseResult = this->final->codegen(cgi);
		cgi->popScope();

		fir::Value* v = elseResult.result.first;

		if(phi)
			phi->addIncoming(v, falseb);
	}


	if(!this->final || elseResult.type != ResultType::BreakCodegen)
		cgi->builder.CreateUnCondBranch(merge), didMerge = true;


	if(didMerge)
	{
		cgi->builder.setCurrentBlock(merge);
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










