// ControlFlowCodegen.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "../include/ast.h"
#include "../include/codegen.h"

#include "llvm/IR/Function.h"

using namespace Ast;
using namespace Codegen;



static void codeGenRecursiveIf(CodegenInstance* cgi, llvm::Function* func, std::deque<std::pair<Expr*, BracedBlock*>> pairs, llvm::BasicBlock* merge, llvm::PHINode* phi, bool* didCreateMerge)
{
	if(pairs.size() == 0)
		return;

	llvm::BasicBlock* t = llvm::BasicBlock::Create(cgi->getContext(), "trueCaseR", func);
	llvm::BasicBlock* f = llvm::BasicBlock::Create(cgi->getContext(), "falseCaseR");

	llvm::Value* cond = pairs.front().first->codegen(cgi).result.first;


	llvm::Type* apprType = cgi->getLlvmType(pairs.front().first);
	cond = cgi->builder.CreateICmpNE(cond, llvm::Constant::getNullValue(apprType), "ifCond");


	cgi->builder.CreateCondBr(cond, t, f);
	cgi->builder.SetInsertPoint(t);

	Result_t blockResult(0, 0);
	llvm::Value* val = nullptr;
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

Result_t IfStmt::codegen(CodegenInstance* cgi, llvm::Value* lhsPtr, llvm::Value* rhs)
{
	iceAssert(this->cases.size() > 0);

	llvm::Value* firstCond = this->cases[0].first->codegen(cgi).result.first;
	llvm::Type* apprType = cgi->getLlvmType(this->cases[0].first);

	// printf("if: %s\n%s\n", cgi->getReadableType(firstCond).c_str(), cgi->printAst(this->cases[0].first).c_str());
	firstCond = cgi->builder.CreateICmpNE(firstCond, llvm::Constant::getNullValue(apprType), "ifCond");



	llvm::Function* func = cgi->builder.GetInsertBlock()->getParent();
	iceAssert(func);

	llvm::BasicBlock* trueb = llvm::BasicBlock::Create(cgi->getContext(), "trueCase", func);
	llvm::BasicBlock* falseb = llvm::BasicBlock::Create(cgi->getContext(), "falseCase", func);
	llvm::BasicBlock* merge = llvm::BasicBlock::Create(cgi->getContext(), "merge", func);

	// create the first conditional
	cgi->builder.CreateCondBr(firstCond, trueb, falseb);


	bool didMerge = false;

	// emit code for the first block
	llvm::Value* truev = nullptr;
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

	llvm::BasicBlock* curblk = cgi->builder.GetInsertBlock();
	cgi->builder.SetInsertPoint(merge);

	// llvm::PHINode* phi = builder.CreatePHI(llvm::Type::getVoidTy(getContext()), this->cases.size() + (this->final ? 1 : 0));

	llvm::PHINode* phi = nullptr;

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

		llvm::Value* v = elseResult.result.first;

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











