// ControlFlowCodegen.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "../include/ast.h"
#include "../include/codegen.h"

#include "llvm/IR/Function.h"

using namespace Ast;
using namespace Codegen;



static void codeGenRecursiveIf(CodegenInstance* cgi, fir::Function* func, std::deque<std::pair<Expr*, BracedBlock*>> pairs, fir::BasicBlock* merge, fir::PHINode* phi, bool* didCreateMerge)
{
	if(pairs.size() == 0)
		return;

	fir::BasicBlock* t = fir::BasicBlock::Create(cgi->getContext(), "trueCaseR", func);
	fir::BasicBlock* f = fir::BasicBlock::Create(cgi->getContext(), "falseCaseR");

	fir::Value* cond = pairs.front().first->codegen(cgi).result.first;


	fir::Type* apprType = cgi->getLlvmType(pairs.front().first);
	cond = cgi->builder.CreateICmpNE(cond, fir::Constant::getNullValue(apprType), "ifCond");


	cgi->builder.CreateCondBr(cond, t, f);
	cgi->builder.SetInsertPoint(t);

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
		cgi->builder.CreateBr(merge), *didCreateMerge = true;


	// now the false case...
	// set the insert point to the false case, then go again.
	cgi->builder.SetInsertPoint(f);

	// recursively call ourselves
	pairs.pop_front();
	codeGenRecursiveIf(cgi, func, pairs, merge, phi, didCreateMerge);

	// once that's done, we can add the false-case block to the func
	func->getBasicBlockList().push_back(f);
}

Result_t IfStmt::codegen(CodegenInstance* cgi, fir::Value* lhsPtr, fir::Value* rhs)
{
	iceAssert(this->cases.size() > 0);

	fir::Value* firstCond = this->cases[0].first->codegen(cgi).result.first;
	fir::Type* apprType = cgi->getLlvmType(this->cases[0].first);

	// printf("if: %s\n%s\n", cgi->getReadableType(firstCond).c_str(), cgi->printAst(this->cases[0].first).c_str());
	firstCond = cgi->builder.CreateICmpNE(firstCond, fir::Constant::getNullValue(apprType), "ifCond");



	fir::Function* func = cgi->builder.GetInsertBlock()->getParent();
	iceAssert(func);

	fir::BasicBlock* trueb = fir::BasicBlock::Create(cgi->getContext(), "trueCase", func);
	fir::BasicBlock* falseb = fir::BasicBlock::Create(cgi->getContext(), "falseCase", func);
	fir::BasicBlock* merge = fir::BasicBlock::Create(cgi->getContext(), "merge", func);

	// create the first conditional
	cgi->builder.CreateCondBr(firstCond, trueb, falseb);


	bool didMerge = false;

	// emit code for the first block
	fir::Value* truev = nullptr;
	{
		cgi->builder.SetInsertPoint(trueb);

		// push a new symtab
		cgi->pushScope();

		// generate the statements inside
		Result_t cresult = this->cases[0].second->codegen(cgi);
		truev = cresult.result.first;
		cgi->popScope();

		if(cresult.type != ResultType::BreakCodegen)
			cgi->builder.CreateBr(merge), didMerge = true;
	}



	// now for the clusterfuck.
	// to support if-elseif-elseif-elseif-...-else, we need to essentially compound/cascade conditionals in the 'else' block
	// of the if statement.

	cgi->builder.SetInsertPoint(falseb);

	auto c1 = this->cases.front();
	this->cases.pop_front();

	fir::BasicBlock* curblk = cgi->builder.GetInsertBlock();
	cgi->builder.SetInsertPoint(merge);

	// fir::PHINode* phi = builder.CreatePHI(fir::Type::getVoidTy(getContext()), this->cases.size() + (this->final ? 1 : 0));

	fir::PHINode* phi = nullptr;

	if(phi)
		phi->addIncoming(truev, trueb);

	cgi->builder.SetInsertPoint(curblk);
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
		cgi->builder.CreateBr(merge), didMerge = true;


	if(didMerge)
	{
		cgi->builder.SetInsertPoint(merge);
	}
	else
	{
		merge->eraseFromParent();
	}

	// restore.
	this->cases = this->_cases;
	return Result_t(0, 0);
}











