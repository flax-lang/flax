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
	fir::Value* firstCond = cs->oneWayAutocast(this->cases.front().cond->codegen(cs, fir::Type::getBool()), fir::Type::getBool()).value;
	iceAssert(firstCond);

	if(!firstCond->getType()->isBoolType())
		error(this->cases.front().cond, "Non-boolean type '%s' cannot be used as a conditional", firstCond->getType());


	// do a comparison
	fir::Value* cmpRes = cs->irb.ICmpEQ(firstCond, fir::ConstantBool::get(true));
	cs->irb.CondBranch(cmpRes, trueblk, elseblk);


	// now, then.
	cs->irb.setCurrentBlock(trueblk);
	{
		auto c = this->cases.front();
		c.body->codegen(cs);

		if(!cs->irb.getCurrentBlock()->isTerminated())
			cs->irb.UnCondBranch(mergeblk);
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
			auto cond = cs->oneWayAutocast(elif.cond->codegen(cs, fir::Type::getBool()), fir::Type::getBool()).value;
			iceAssert(cond);

			if(!cond->getType()->isBoolType())
				error(elif.cond, "Non-boolean type '%s' cannot be used as a conditional", cond->getType());

			// ok
			auto trueblk = cs->irb.addNewBlockAfter("trueCaseR", cs->irb.getCurrentBlock());
			auto falseblkr = cs->irb.addNewBlockAfter("falseCaseR", cs->irb.getCurrentBlock());

			fir::Value* cmpr = cs->irb.ICmpEQ(cond, fir::ConstantBool::get(true));

			cs->irb.CondBranch(cmpr, trueblk, falseblkr);


			cs->irb.setCurrentBlock(trueblk);
			{
				elif.body->codegen(cs);

				if(!cs->irb.getCurrentBlock()->isTerminated())
					cs->irb.UnCondBranch(mergeblk);
			}

			cs->irb.setCurrentBlock(falseblkr);
			{
				// ok, do the next thing.
				// if we're the last block, then gtfo and branch to merge
				if(elif == remaining.back())
				{
					if(!cs->irb.getCurrentBlock()->isTerminated())
						cs->irb.UnCondBranch(mergeblk);

					break;
				}
			}
		}
	}


	cs->irb.setCurrentBlock(elseblk);
	{
		if(this->elseCase)
			this->elseCase->codegen(cs);

		if(elseblk != mergeblk && !cs->irb.getCurrentBlock()->isTerminated())
			cs->irb.UnCondBranch(mergeblk);
	}

	cs->irb.setCurrentBlock(mergeblk);

	return CGResult(0);
}













static void doBlockEndThings(cgn::CodegenState* cs, cgn::ControlFlowPoint cfp)
{
	// then do the defers
	for(auto stmt : cfp.block->deferred)
		stmt->codegen(cs);

	for(auto v : cfp.refCountedValues)
		cs->decrementRefCount(v);

	for(auto p : cfp.refCountedPointers)
		cs->decrementRefCount(cs->irb.Load(p));
}

CGResult sst::BreakStmt::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	auto bp = cs->getCurrentCFPoint().breakPoint;
	iceAssert(bp);

	// do the necessary
	doBlockEndThings(cs, cs->getCurrentCFPoint());
	cs->irb.UnCondBranch(bp);

	return CGResult(0, 0, CGResult::VK::Break);
}

CGResult sst::ContinueStmt::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	auto cp = cs->getCurrentCFPoint().continuePoint;
	iceAssert(cp);

	// do the necessary
	doBlockEndThings(cs, cs->getCurrentCFPoint());
	cs->irb.UnCondBranch(cp);

	return CGResult(0, 0, CGResult::VK::Continue);
}



CGResult sst::ReturnStmt::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	// check if we have a value, and whether it's refcounted
	// if so, inflate its refcount so it doesn't get deallocated and can survive

	if(this->value)
	{
		auto v = this->value->codegen(cs, this->expectedType).value;
		if(cs->isRefCountedType(v->getType()))
			cs->incrementRefCount(v);

		doBlockEndThings(cs, cs->getCurrentCFPoint());
		cs->irb.Return(v);
	}
	else
	{
		doBlockEndThings(cs, cs->getCurrentCFPoint());

		iceAssert(this->expectedType->isVoidType());
		cs->irb.ReturnVoid();
	}

	return CGResult(0, 0, CGResult::VK::Break);
}






CGResult sst::Block::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	// auto rsn = cs->setNamespace(this->scope);
	// defer(cs->restoreNamespace(rsn));

	bool broke = false;
	bool cont = false;
	for(auto stmt : this->statements)
	{
		auto res = stmt->codegen(cs);
		if(res.kind == CGResult::VK::Break || res.kind == CGResult::VK::Continue)
		{
			broke = true;
			cont = (res.kind == CGResult::VK::Continue);
			break;
		}
	}


	if(!broke)
	{
		for(auto stmt : this->deferred)
			stmt->codegen(cs);

		// then decrement all the refcounts
		for(auto v : cs->getRefCountedValues())
			cs->decrementRefCount(v);

		for(auto p : cs->getRefCountedPointers())
			cs->decrementRefCount(cs->irb.Load(p));
	}

	return CGResult(0);
}




















