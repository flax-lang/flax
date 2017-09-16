// controlflow.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "sst.h"
#include "codegen.h"

#define dcast(t, v)		dynamic_cast<t*>(v)

// just a tmp thing
static bool operator == (const sst::IfStmt::Case& a, const sst::IfStmt::Case& b)
{
	return (a.body == b.body && a.cond == b.cond && a.inits == b.inits);
}

CGResult sst::IfStmt::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	cs->enterNamespace(this->generatedScopeName);
	defer(cs->leaveNamespace());

	auto trueblk = cs->irb.addNewBlockAfter("trueCase", cs->irb.getCurrentBlock());
	auto mergeblk = cs->irb.addNewBlockAfter("mergeCase", cs->irb.getCurrentBlock());

	fir::IRBlock* elseblk = 0;
	if(this->elseCase)	elseblk = cs->irb.addNewBlockAfter("elseCase", trueblk);
	else				elseblk = mergeblk;


	// first we gotta do all the inits of all the cases first.
	// we're already in our own scope, so it shouldn't matter.

	for(auto c : this->cases)
		for(auto i : c.inits)
			i->codegen(cs);

	// at the current place, first do the cond.
	iceAssert(this->cases.size() > 0);
	fir::Value* firstCond = this->cases.front().cond->codegen(cs, fir::Type::getBool()).value;

	if(!firstCond->getType()->isBoolType())
		error(this->cases.front().cond, "Non-boolean type '%s' cannot be used as a conditional", firstCond->getType()->str());


	// do a comparison
	fir::Value* cmpRes = cs->irb.CreateICmpEQ(firstCond, fir::ConstantBool::get(true));
	cs->irb.CreateCondBranch(cmpRes, trueblk, elseblk);


	// now, then.
	cs->irb.setCurrentBlock(trueblk);
	{
		auto c = this->cases.front();
		c.body->codegen(cs);

		cs->irb.CreateUnCondBranch(mergeblk);
	}

	// ok -- we don't really need to do it recursively, do we?
	auto remaining = std::vector<Case>(this->cases.begin() + 1, this->cases.end());
	if(remaining.size() > 0)
	{
		// this block serves the purpose of initialising the conditions and stuff
		auto falseblk = cs->irb.addNewBlockAfter("falseCase", cs->irb.getCurrentBlock());
		cs->irb.setCurrentBlock(falseblk);

		for(auto elif : remaining)
		{
			auto cond = elif.cond->codegen(cs, fir::Type::getBool()).value;
			if(!cond->getType()->isBoolType())
				error(elif.cond, "Non-boolean type '%s' cannot be used as a conditional", cond->getType()->str());

			// ok
			auto trueblk = cs->irb.addNewBlockAfter("trueCaseR", cs->irb.getCurrentBlock());
			auto falseblkr = cs->irb.addNewBlockAfter("falseCaseR", cs->irb.getCurrentBlock());

			fir::Value* cmpr = cs->irb.CreateICmpEQ(cond, fir::ConstantBool::get(true));

			cs->irb.CreateCondBranch(cmpr, trueblk, falseblkr);

			cs->irb.setCurrentBlock(trueblk);
			{
				elif.body->codegen(cs);

				cs->irb.CreateUnCondBranch(mergeblk);
			}

			cs->irb.setCurrentBlock(falseblkr);
			{
				// ok, do the next thing.
				// if we're the last block, then gtfo and branch to merge
				if(elif == remaining.back())
				{
					cs->irb.CreateUnCondBranch(mergeblk);
					break;
				}
			}
		}
	}


	cs->irb.setCurrentBlock(elseblk);
	{
		if(this->elseCase)
			this->elseCase->codegen(cs);

		if(elseblk != mergeblk)
			cs->irb.CreateUnCondBranch(mergeblk);
	}

	cs->irb.setCurrentBlock(mergeblk);

	return CGResult(0);
}





CGResult sst::ReturnStmt::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	if(this->value)
	{
		auto v = this->value->codegen(cs, this->expectedType).value;
		cs->irb.CreateReturn(v);
	}
	else
	{
		iceAssert(this->expectedType->isVoidType());
		cs->irb.CreateReturnVoid();
	}

	return CGResult(0);
}

































