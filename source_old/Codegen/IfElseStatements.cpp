// ControlFlowCodegen.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "ast.h"
#include "codegen.h"

using namespace Ast;
using namespace Codegen;

using CondTuple = std::vector<std::tuple<Expr*, BracedBlock*, std::vector<Expr*>>>;

static void codeGenRecursiveIf(CodegenInstance* cgi, fir::Function* func, CondTuple pairs,
	fir::IRBlock* merge, fir::PHINode* phi, bool* didCreateMerge, bool* allBroke)
{
	if(pairs.size() == 0)
		return;

	fir::IRBlock* t = cgi->irb.addNewBlockInFunction("trueCaseR", func);
	fir::IRBlock* f = cgi->irb.addNewBlockInFunction("falseCaseR", func);


	// this is here (and not paired with the popScope() below) because we need to keep the scope tight
	// *but* at the same time the inits need to be ready in time to be codegened as the condition
	cgi->pushScope();

	for(auto in : std::get<2>(pairs.front()))
		in->codegen(cgi);


	fir::Value* cond = std::get<0>(pairs.front())->codegen(cgi).value;
	if(cond->getType() != fir::Type::getBool())
	{
		error(std::get<0>(pairs.front()), "Non-boolean type '%s' cannot be used as the conditional for an if statement",
			cond->getType()->str().c_str());
	}

	cond = cgi->irb.CreateICmpNEQ(cond, fir::ConstantValue::getZeroValue(cond->getType()));


	cgi->irb.CreateCondBranch(cond, t, f);
	cgi->irb.setCurrentBlock(t);

	Result_t blockResult(0, 0);
	fir::Value* val = nullptr;
	{
		blockResult = std::get<1>(pairs.front())->codegen(cgi);
		val = blockResult.value;
		cgi->popScope();
	}

	if(phi)
		phi->addIncoming(val, t);

	// check if the last expr of the block is a return
	if(blockResult.type != ResultType::BreakCodegen)
		cgi->irb.CreateUnCondBranch(merge), *didCreateMerge = true;

	*allBroke = *allBroke && (blockResult.type == ResultType::BreakCodegen);

	// now the false case...
	// set the insert point to the false case, then go again.
	cgi->irb.setCurrentBlock(f);

	// recursively call ourselves
	pairs.erase(pairs.begin());
	codeGenRecursiveIf(cgi, func, pairs, merge, phi, didCreateMerge, allBroke);
}

Result_t IfStmt::codegen(CodegenInstance* cgi, fir::Type* extratype, fir::Value* target)
{
	iceAssert(this->cases.size() > 0);

	auto backupCases = this->cases;


	fir::Function* func = cgi->irb.getCurrentBlock()->getParentFunction();
	iceAssert(func);

	fir::IRBlock* trueb = cgi->irb.addNewBlockInFunction("trueCase", func);
	fir::IRBlock* falseb = cgi->irb.addNewBlockInFunction("falseCase", func);
	fir::IRBlock* merge = cgi->irb.addNewBlockInFunction("merge", func);


	// push a new symtab
	cgi->pushScope();

	// generate the statements inside

	// first, do the inits (inside the new pushScope)
	for(auto in : std::get<2>(this->cases[0]))
		in->codegen(cgi);


	fir::Value* firstCond = std::get<0>(this->cases[0])->codegen(cgi).value;
	if(firstCond->getType() != fir::Type::getBool())
	{
		error(std::get<0>(this->cases[0]), "Non-boolean type '%s' cannot be used as the conditional for an if statement",
			firstCond->getType()->str().c_str());
	}

	firstCond = cgi->irb.CreateICmpNEQ(firstCond, fir::ConstantValue::getZeroValue(firstCond->getType()));


	// create the first conditional
	cgi->irb.CreateCondBranch(firstCond, trueb, falseb);


	bool didMerge = false;
	bool allBroke = true;

	// emit code for the first block
	fir::Value* truev = nullptr;
	{
		cgi->irb.setCurrentBlock(trueb);

		// then do the block
		ResultType rt;
		std::tie(truev, rt) = std::get<1>(this->cases[0])->codegen(cgi);
		cgi->popScope();

		if(rt != ResultType::BreakCodegen)
			cgi->irb.CreateUnCondBranch(merge), didMerge = true;

		allBroke = allBroke && (rt == ResultType::BreakCodegen);

		// warn(this->cases[0].first, "%d / %d", rt, didMerge);
	}



	// now for the clusterfuck.
	// to support if-elseif-elseif-elseif-...-else, we need to essentially compound/cascade conditionals in the 'else' block
	// of the if statement.

	cgi->irb.setCurrentBlock(falseb);

	// auto c1 = this->cases.front();
	this->cases.erase(this->cases.begin());

	fir::IRBlock* curblk = cgi->irb.getCurrentBlock();
	cgi->irb.setCurrentBlock(merge);

	fir::PHINode* phi = nullptr;
	if(phi) phi->addIncoming(truev, trueb);

	cgi->irb.setCurrentBlock(curblk);
	codeGenRecursiveIf(cgi, func, this->cases, merge, phi, &didMerge, &allBroke);


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

	allBroke = allBroke && (elsetype == ResultType::BreakCodegen);


	if(didMerge)
	{
		cgi->irb.setCurrentBlock(merge);
	}
	else
	{
		merge->eraseFromParentFunction();
	}

	// restore.
	this->cases = backupCases;
	return Result_t(0, 0, allBroke ? ResultType::BreakCodegen : ResultType::Normal);
}

fir::Type* IfStmt::getType(CodegenInstance* cgi, fir::Type* extratype, bool allowFail)
{
	return 0;
}










