// controlflow.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
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

	if(!cs->getCurrentBlockPoint().block->elideMergeBlock)
		mergeblk = cs->irb.addNewBlockAfter("mergeCase-" + this->loc.shortString(), trueblk);

	else
		iceAssert(this->elseCase);

	fir::IRBlock* elseblk = 0;
	if(this->elseCase)	elseblk = cs->irb.addNewBlockAfter("elseCase-" + this->elseCase->loc.shortString(), trueblk);
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
	auto restore = cs->irb.getCurrentBlock();

	//! ACHTUNG !
	//* we are not finished here; we will come back and insert an appropriate branch later.


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
		auto falseblk = cs->irb.addNewBlockAfter("falseCase", trueblk);
		{
			//* the patching -- if we have 'else-if' cases.
			cs->irb.setCurrentBlock(restore);
			cs->irb.CondBranch(cmpRes, trueblk, falseblk);
		}

		cs->irb.setCurrentBlock(falseblk);

		for(auto elif : remaining)
		{
			auto cond = cs->oneWayAutocast(elif.cond->codegen(cs, fir::Type::getBool()), fir::Type::getBool()).value;
			iceAssert(cond);

			if(!cond->getType()->isBoolType())
				error(elif.cond, "Non-boolean type '%s' cannot be used as a conditional", cond->getType());

			// ok
			auto trueblk = cs->irb.addNewBlockAfter("trueCase-" + elif.cond->loc.shortString(), cs->irb.getCurrentBlock());
			auto falseblkr = cs->irb.addNewBlockAfter("falseCase-" + elif.cond->loc.shortString(), cs->irb.getCurrentBlock());

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
						cs->irb.UnCondBranch(elseblk);

					break;
				}
			}
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

	cs->irb.setCurrentBlock(mergeblk);

	return CGResult(0);
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
	#if DEBUG_ARRAY_REFCOUNTING | DEBUG_STRING_REFCOUNTING
	{
		cs->printIRDebugMessage("\n! CTRLFLOW: at: " + cfp.block->loc.shortString() + "\n{", { });
		cs->pushIRDebugIndentation();
	}
	#endif

	// then do the defers
	for(auto stmt : cfp.block->deferred)
		stmt->codegen(cs);

	for(auto v : bp.refCountedValues)
		cs->decrementRefCount(v);

	for(auto p : bp.refCountedPointers)
		cs->decrementRefCount(cs->irb.ReadPtr(p));


	#if DEBUG_ARRAY_REFCOUNTING | DEBUG_STRING_REFCOUNTING
	{
		cs->popIRDebugIndentation();
		cs->printIRDebugMessage("}", { });
	}
	#endif
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
	// check if we have a value, and whether it's refcounted
	// if so, inflate its refcount so it doesn't get deallocated and can survive

	if(this->value)
	{
		auto v = this->value->codegen(cs, this->expectedType).value;
		if(cs->isRefCountedType(v->getType()))
			cs->incrementRefCount(v);

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


	if(!broke)
	{
		#if DEBUG_ARRAY_REFCOUNTING | DEBUG_STRING_REFCOUNTING
		{
			cs->printIRDebugMessage("\n! BLOCKEND: at: " + this->loc.shortString() + "\n{", { });
			cs->pushIRDebugIndentation();
		}
		#endif

		for(auto it = this->deferred.rbegin(); it != this->deferred.rend(); it++)
			(*it)->codegen(cs);

		// then decrement all the refcounts
		for(auto v : cs->getRefCountedValues())
			cs->decrementRefCount(v);

		for(auto p : cs->getRefCountedPointers())
			cs->decrementRefCount(cs->irb.ReadPtr(p));


		#if DEBUG_ARRAY_REFCOUNTING | DEBUG_STRING_REFCOUNTING
		{
			cs->popIRDebugIndentation();
			cs->printIRDebugMessage("}", { });
		}
		#endif
	}

	return CGResult(0);
}




















