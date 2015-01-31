// ControlFlowCodegen.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "../include/ast.h"
#include "../include/codegen.h"
#include "../include/llvm_all.h"

using namespace Ast;
using namespace Codegen;



void codeGenRecursiveIf(CodegenInstance* cgi, llvm::Function* func, std::deque<std::pair<Expr*, BracedBlock*>> pairs, llvm::BasicBlock* merge, llvm::PHINode* phi, bool* didCreateMerge)
{
	if(pairs.size() == 0)
		return;

	llvm::BasicBlock* t = llvm::BasicBlock::Create(cgi->getContext(), "trueCaseR", func);
	llvm::BasicBlock* f = llvm::BasicBlock::Create(cgi->getContext(), "falseCaseR");

	llvm::Value* cond = pairs.front().first->codegen(cgi).result.first;


	VarType apprType = cgi->determineVarType(pairs.front().first);
	if(apprType != VarType::Bool)
		cond = cgi->mainBuilder.CreateICmpNE(cond, llvm::ConstantInt::get(cgi->getContext(), llvm::APInt(pow(2, (int) apprType % 4) * 8, 0, apprType > VarType::Int64)), "ifCond");

	else
		cond = cgi->mainBuilder.CreateICmpNE(cond, llvm::ConstantInt::get(cgi->getContext(), llvm::APInt(1, false, true)));



	cgi->mainBuilder.CreateCondBr(cond, t, f);
	cgi->mainBuilder.SetInsertPoint(t);

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
		cgi->mainBuilder.CreateBr(merge), *didCreateMerge = true;


	// now the false case...
	// set the insert point to the false case, then go again.
	cgi->mainBuilder.SetInsertPoint(f);

	// recursively call ourselves
	pairs.pop_front();
	codeGenRecursiveIf(cgi, func, pairs, merge, phi, didCreateMerge);

	// once that's done, we can add the false-case block to the func
	func->getBasicBlockList().push_back(f);
}

Result_t If::codegen(CodegenInstance* cgi)
{
	assert(this->cases.size() > 0);
	llvm::Value* firstCond = this->cases[0].first->codegen(cgi).result.first;
	VarType apprType = cgi->determineVarType(this->cases[0].first);

	if(apprType != VarType::Bool)
	{
		firstCond = cgi->mainBuilder.CreateICmpNE(firstCond, llvm::ConstantInt::get(cgi->getContext(), llvm::APInt(pow(2, (int) apprType % 4) * 8, 0, apprType > VarType::Int64)), "ifCond");
	}
	else
	{
		firstCond = cgi->mainBuilder.CreateICmpNE(firstCond, llvm::ConstantInt::get(cgi->getContext(), llvm::APInt(1, false, true)),
			"ifCond");
	}

	llvm::Function* func = cgi->mainBuilder.GetInsertBlock()->getParent();
	llvm::BasicBlock* trueb = llvm::BasicBlock::Create(cgi->getContext(), "trueCase", func);
	llvm::BasicBlock* falseb = llvm::BasicBlock::Create(cgi->getContext(), "falseCase");
	llvm::BasicBlock* merge = llvm::BasicBlock::Create(cgi->getContext(), "merge");

	// create the first conditional
	cgi->mainBuilder.CreateCondBr(firstCond, trueb, falseb);


	bool didMerge = false;

	// emit code for the first block
	llvm::Value* truev = nullptr;
	{
		cgi->mainBuilder.SetInsertPoint(trueb);

		// push a new symtab
		cgi->pushScope();

		// generate the statements inside
		Result_t cresult = this->cases[0].second->codegen(cgi);
		truev = cresult.result.first;
		cgi->popScope();

		if(cresult.type != ResultType::BreakCodegen)
			cgi->mainBuilder.CreateBr(merge), didMerge = true;
	}



	// now for the clusterfuck.
	// to support if-elseif-elseif-elseif-...-else, we need to essentially compound/cascade conditionals in the 'else' block
	// of the if statement.

	cgi->mainBuilder.SetInsertPoint(falseb);

	auto c1 = this->cases.front();
	this->cases.pop_front();

	llvm::BasicBlock* curblk = cgi->mainBuilder.GetInsertBlock();
	cgi->mainBuilder.SetInsertPoint(merge);

	// llvm::PHINode* phi = mainBuilder.CreatePHI(llvm::Type::getVoidTy(getContext()), this->cases.size() + (this->final ? 1 : 0));

	llvm::PHINode* phi = nullptr;

	if(phi)
		phi->addIncoming(truev, trueb);

	cgi->mainBuilder.SetInsertPoint(curblk);
	codeGenRecursiveIf(cgi, func, std::deque<std::pair<Expr*, BracedBlock*>>(this->cases), merge, phi, &didMerge);

	func->getBasicBlockList().push_back(falseb);

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
		cgi->mainBuilder.CreateBr(merge), didMerge = true;


	if(didMerge)
	{
		func->getBasicBlockList().push_back(merge);
		cgi->mainBuilder.SetInsertPoint(merge);
	}

	return Result_t(0, 0);
}











