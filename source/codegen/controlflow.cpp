// controlflow.cpp
// Copyright (c) 2014 - 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include "sst.h"
#include "codegen.h"
#include "gluecode.h"

// just a tmp thing
static bool operator == (const sst::IfStmt::Case& a, const sst::IfStmt::Case& b)
{
	return (a.body == b.body && a.cond == b.cond && a.inits == b.inits);
}

CGResult sst::IfStmt::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	fir::IRBlock* mergeblk = 0;
	auto trueblk = cs->irb.addNewBlockAfter("trueCase-" + this->loc.shortString(), cs->irb.getCurrentBlock());

	if(!this->elideMergeBlock)
		mergeblk = cs->irb.addNewBlockAfter("mergeCase-" + this->loc.shortString(), trueblk);

	else
		iceAssert(this->elseCase);

	fir::IRBlock* elseblk = 0;
	if(this->elseCase)  elseblk = cs->irb.addNewBlockAfter("elseCase-" + this->elseCase->loc.shortString(), trueblk);
	else                elseblk = mergeblk;

	// first we gotta do all the inits of all the cases first.
	// we're already in our own scope, so it shouldn't matter.

	for(auto c : this->cases)
		for(auto i : c.inits)
			i->codegen(cs);

	// at the current place, first do the cond.
	iceAssert(this->cases.size() > 0);
	fir::Value* firstCond = cs->oneWayAutocast(this->cases.front().cond->codegen(cs, fir::Type::getBool()).value, fir::Type::getBool());
	iceAssert(firstCond);

	if(!firstCond->getType()->isBoolType())
		error(this->cases.front().cond, "non-boolean type '%s' cannot be used as a conditional", firstCond->getType());


	// do a comparison
	// don't be stupid and cmp bool == true
	fir::Value* cmpRes = firstCond;

	auto restore = cs->irb.getCurrentBlock();

	//! ACHTUNG !
	//* we are not finished here; we will come back and insert an appropriate branch later.


	// now, then.
	cs->irb.setCurrentBlock(trueblk);
	{
		auto c = this->cases.front();
		c.body->codegen(cs);

		if(cs->irb.getCurrentBlock() != nullptr && !cs->irb.getCurrentBlock()->isTerminated())
			cs->irb.UnCondBranch(mergeblk);
	}

	// ok -- we don't really need to do it recursively, do we?
	auto remaining = std::vector<Case>(this->cases.begin() + 1, this->cases.end());
	if(remaining.size() > 0)
	{
		// this block serves the purpose of initialising the conditions and stuff
		auto falseblk = cs->irb.addNewBlockAfter("falseCase-" + remaining[0].body->loc.shortString(), trueblk);
		{
			//* the patching -- if we have 'else-if' cases.
			cs->irb.setCurrentBlock(restore);
			cs->irb.CondBranch(cmpRes, trueblk, falseblk);
		}

		cs->irb.setCurrentBlock(falseblk);

		for(auto elif : remaining)
		{
			auto cond = cs->oneWayAutocast(elif.cond->codegen(cs, fir::Type::getBool()).value, fir::Type::getBool());
			iceAssert(cond);

			if(!cond->getType()->isBoolType())
				error(elif.cond, "non-boolean type '%s' cannot be used as a conditional", cond->getType());

			// ok
			auto trueblk = cs->irb.addNewBlockAfter("trueCase-" + elif.body->loc.shortString(), cs->irb.getCurrentBlock());
			fir::IRBlock* falseblkr = 0;
			if(elif == remaining.back())    falseblkr = elseblk;
			else                            falseblkr = cs->irb.addNewBlockAfter("falseCase-" + elif.body->loc.shortString(), cs->irb.getCurrentBlock());

			fir::Value* cmpr = cond;

			cs->irb.CondBranch(cmpr, trueblk, falseblkr);


			cs->irb.setCurrentBlock(trueblk);
			{
				elif.body->codegen(cs);

				if(!cs->irb.getCurrentBlock()->isTerminated())
					cs->irb.UnCondBranch(mergeblk);
			}

			cs->irb.setCurrentBlock(falseblkr);
		}
	}
	else
	{
		//* the patching -- if we have no 'else-if' cases.
		cs->irb.setCurrentBlock(restore);
		cs->irb.CondBranch(cmpRes, trueblk, elseblk);
	}


	cs->irb.setCurrentBlock(elseblk);
	{
		if(this->elseCase)
			this->elseCase->codegen(cs);

		if(elseblk != mergeblk && !cs->irb.getCurrentBlock()->isTerminated())
			cs->irb.UnCondBranch(mergeblk);
	}

	if(mergeblk) cs->irb.setCurrentBlock(mergeblk);

	// if we were supposed to elide the merge block, it means we have no more stuff after us.
	// tell our parent block this -- so that it can skip any extraneous codegen, eg:
	// fn x() { if true { return } else { return } return }
	// the last 'return' will cause a problem in IR generation if we don't skip it.

	return CGResult(0, this->elideMergeBlock ? CGResult::VK::EarlyOut : CGResult::VK::Normal);
}

std::vector<sst::Block*> sst::IfStmt::getBlocks()
{
	std::vector<sst::Block*> ret;
	for(auto c : this->cases)
		ret.push_back(c.body);

	if(this->elseCase)
		ret.push_back(this->elseCase);

	return ret;
}











static void doBlockEndThings(cgn::CodegenState* cs, const cgn::ControlFlowPoint& cfp, const cgn::BlockPoint& bp)
{
	// then do the defers
	for(auto stmt : cfp.block->deferred)
		stmt->_codegen(cs);

	for(auto c : bp.raiiValues)
		cs->callDestructor(c);
}

CGResult sst::BreakStmt::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	auto bp = cs->getCurrentCFPoint().breakPoint;
	iceAssert(bp);

	// do the necessary
	doBlockEndThings(cs, cs->getCurrentCFPoint(), cs->getCurrentBlockPoint());
	cs->irb.UnCondBranch(bp);

	return CGResult(0, CGResult::VK::EarlyOut);
}

CGResult sst::ContinueStmt::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	auto cp = cs->getCurrentCFPoint().continuePoint;
	iceAssert(cp);

	// do the necessary
	doBlockEndThings(cs, cs->getCurrentCFPoint(), cs->getCurrentBlockPoint());
	cs->irb.UnCondBranch(cp);

	return CGResult(0, CGResult::VK::EarlyOut);
}



CGResult sst::ReturnStmt::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	if(this->value)
	{
		auto v = this->value->codegen(cs, this->expectedType).value;
		if(v->getType() != this->expectedType)
			v = cs->oneWayAutocast(v, this->expectedType);

		//! RAII: COPY CONSTRUCTOR CALL
		//? the copy constructor is called when a function returns an object by value
		if(v->getType()->isClassType())
			v = cs->copyRAIIValue(v);

		doBlockEndThings(cs, cs->getCurrentCFPoint(), cs->getCurrentBlockPoint());

		cs->irb.Return(v);
	}
	else
	{
		doBlockEndThings(cs, cs->getCurrentCFPoint(), cs->getCurrentBlockPoint());

		iceAssert(this->expectedType->isVoidType());
		cs->irb.ReturnVoid();
	}

	return CGResult(0, CGResult::VK::EarlyOut);
}






CGResult sst::Block::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	cs->enterBlock(this);
	defer(cs->leaveBlock());

	if(this->preBodyCode)
		this->preBodyCode();

	bool broke = false;
	for(auto stmt : this->statements)
	{
		auto res = stmt->codegen(cs);
		if(res.kind == CGResult::VK::EarlyOut)
		{
			broke = true;
			break;
		}
	}

	if(this->postBodyCode)
		this->postBodyCode();

	// the reason we check for !broke here before doing the block cleanup is because BreakStmt
	// and ContinueStmt will do it too.
	if(!broke)
	{
		//* this duplicates stuff from doBlockEndThings!!
		for(auto it = this->deferred.rbegin(); it != this->deferred.rend(); it++)
			(*it)->_codegen(cs);

		for(auto c : cs->getCurrentBlockPoint().raiiValues)
			cs->callDestructor(c);
	}

	return CGResult(0);
}




















