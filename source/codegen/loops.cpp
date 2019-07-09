// loops.cpp
// Copyright (c) 2014 - 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include "sst.h"
#include "codegen.h"
#include "mpool.h"

CGResult sst::WhileLoop::_codegen(cgn::CodegenState* cs, fir::Type* inferred)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	if(this->isDoVariant && !this->cond)
	{
		this->body->codegen(cs);
		return CGResult(0);
	}



	auto loop = cs->irb.addNewBlockAfter("loop-" + this->loc.shortString(), cs->irb.getCurrentBlock());
	fir::IRBlock* merge = 0;

	if(!this->elideMergeBlock)
		merge = cs->irb.addNewBlockAfter("merge-" + this->loc.shortString(), cs->irb.getCurrentBlock());

	else if(!this->isDoVariant)
		error(this, "internal error: cannot elide merge block with non-do while loop");



	auto getcond = [](cgn::CodegenState* cs, Expr* c) -> fir::Value* {

		auto cv = cs->oneWayAutocast(c->codegen(cs, fir::Type::getBool()).value, fir::Type::getBool());
		if(cv->getType() != fir::Type::getBool())
			error(c, "non-boolean expression with type '%s' cannot be used as a conditional", cv->getType());

		// ok
		return cv;
	};



	if(this->isDoVariant)
	{
		cs->irb.UnCondBranch(loop);
		cs->irb.setCurrentBlock(loop);

		cs->enterBreakableBody(cgn::ControlFlowPoint(this->body, merge, loop));
		{
			this->body->codegen(cs);
		}
		cs->leaveBreakableBody();

		// if merge == NULL, that means we're suppose to elide the merge block.
		// so, don't insert any more instructions.
		if(merge)
		{
			iceAssert(!cs->irb.getCurrentBlock()->isTerminated());
			if(this->cond)
			{
				iceAssert(this->cond);
				auto condv = getcond(cs, this->cond);
				cs->irb.CondBranch(condv, loop, merge);
			}
			else
			{
				cs->irb.UnCondBranch(merge);
			}
		}
		else
		{
			iceAssert(cs->irb.getCurrentBlock()->isTerminated());
		}
	}
	else
	{
		auto check = cs->irb.addNewBlockAfter("check", cs->irb.getCurrentBlock());
		cs->irb.UnCondBranch(check);
		cs->irb.setCurrentBlock(check);

		// ok
		iceAssert(this->cond);
		iceAssert(merge);
		auto condv = getcond(cs, this->cond);
		cs->irb.CondBranch(condv, loop, merge);

		cs->irb.setCurrentBlock(loop);

		cs->enterBreakableBody(cgn::ControlFlowPoint(this->body, merge, check));
		{
			this->body->codegen(cs);
		}
		cs->leaveBreakableBody();

		// ok, do a jump back to the top
		cs->irb.UnCondBranch(check);
	}

	cs->irb.setCurrentBlock(merge);

	return CGResult(0);
}

std::vector<sst::Block*> sst::WhileLoop::getBlocks()
{
	return { this->body };
}









CGResult sst::ForeachLoop::_codegen(cgn::CodegenState* cs, fir::Type* inferred)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	//? this will change when we get iterators.
	//* but, for now, the basic model is the same for all types -- we get a pointer, we have a starting index, and we have a count.
	//* for ranges, we also have an increment, but for the rest it will be 1.

	/*
		init (current):
			int start = $start
			int end = $end
			int step = $step

		check:
			if start < end goto loop else goto merge

		loop:
			.. do things ..

			start += step
			goto check

		merge:
			.. continue ..
	*/

	// auto prevblock = cs->irb.getCurrentBlock();

	auto check = cs->irb.addNewBlockAfter("check", cs->irb.getCurrentBlock());
	auto loop = cs->irb.addNewBlockAfter("loop", cs->irb.getCurrentBlock());
	auto merge = cs->irb.addNewBlockAfter("merge", cs->irb.getCurrentBlock());

	fir::Value* end = 0;
	fir::Value* step = 0;

	fir::Value* idxptr = cs->irb.StackAlloc(fir::Type::getNativeWord());
	fir::Value* iterptr = cs->irb.StackAlloc(fir::Type::getNativeWord());

	auto [ array, vk ] = this->array->codegen(cs);
	(void) vk;

	if(array->getType()->isRangeType())
	{
		cs->irb.WritePtr(cs->irb.GetRangeLower(array), idxptr);
		end = cs->irb.GetRangeUpper(array);
		step = cs->irb.GetRangeStep(array);

		//* overly verbose explanation:
		/*
			Here's the deal: most of the time we're dealing with arrays, so we just make this a little bit easier in that case.
			we compare start < end as the condition, meaning that 'end' is really a length.

			for ranges however, (since we normalise the half-open range to an open range), we actually want to reach the ending value.
			so, we just add 1 to end. this is independent of the step size.
		*/

		//! note: again for negative ranges, we should be subtracting 1 instead.

		end = cs->irb.Add(end, cs->irb.Select(cs->irb.ICmpGEQ(step, fir::ConstantInt::getNative(0)),
			fir::ConstantInt::getNative(1), fir::ConstantInt::getNative(-1)));
	}
	else
	{
		cs->irb.WritePtr(fir::ConstantInt::getNative(0), idxptr);
		step = fir::ConstantInt::getNative(1);

		if(array->getType()->isDynamicArrayType())
		{
			end = cs->irb.GetSAALength(array);
		}
		else if(array->getType()->isArraySliceType())
		{
			end = cs->irb.GetArraySliceLength(array);
		}
		else if(array->getType()->isStringType())
		{
			end = cs->irb.GetSAALength(array);
		}
		else if(array->getType()->isArrayType())
		{
			end = fir::ConstantInt::getNative(array->getType()->toArrayType()->getArraySize());
		}
		else
		{
			error(this->array, "unsupported type '%s' in foreach loop", array->getType());
		}
	}

	cs->irb.UnCondBranch(check);
	cs->irb.setCurrentBlock(check);

	//! here's some special shit where we handle ranges with start > end
	fir::Value* cond = 0;
	if(array->getType()->isRangeType())
	{
		cond = cs->irb.Select(cs->irb.ICmpGT(step, fir::ConstantInt::getNative(0)),
			cs->irb.ICmpLT(cs->irb.ReadPtr(idxptr), end),		// i < end for step > 0
			cs->irb.ICmpGT(cs->irb.ReadPtr(idxptr), end));		// i > end for step < 0
	}
	else
	{
		cond = cs->irb.ICmpLT(cs->irb.ReadPtr(idxptr), end);
	}

	iceAssert(cond);
	cs->irb.CondBranch(cond, loop, merge);

	cs->irb.setCurrentBlock(loop);
	{
		fir::Value* theptr = 0;
		if(array->getType()->isRangeType())
			theptr = idxptr;

		else if(array->getType()->isDynamicArrayType())
			theptr = cs->irb.GetPointer(cs->irb.GetSAAData(array), cs->irb.ReadPtr(idxptr));

		else if(array->getType()->isArraySliceType())
			theptr = cs->irb.GetPointer(cs->irb.GetArraySliceData(array), cs->irb.ReadPtr(idxptr));

		else if(array->getType()->isStringType())
			theptr = cs->irb.PointerTypeCast(cs->irb.GetPointer(cs->irb.GetSAAData(array), cs->irb.ReadPtr(idxptr)), fir::Type::getInt8Ptr());

		else if(array->getType()->isArrayType())
		{
			fir::Value* arrptr = 0;
			if(array->islvalue())
			{
				arrptr = cs->irb.AddressOf(array, false);
			}
			else
			{
				arrptr = cs->irb.CreateLValue(array->getType());
				cs->irb.Store(array, arrptr);
				arrptr->makeConst();
			}

			theptr = cs->irb.GetPointer(cs->irb.ConstGEP2(arrptr, 0, 0), cs->irb.ReadPtr(idxptr));
		}
		else
		{
			iceAssert(0);
		}


		// make the block
		cs->enterBreakableBody(cgn::ControlFlowPoint(this->body, merge, check));
		{
			// msvc: structured bindings cannot be captured
			// what the FUCK???
			auto _array = array;
			this->body->preBodyCode = [cs, theptr, _array, iterptr, this]() {

				// TODO: is this correct???
				auto res = CGResult(cs->irb.Dereference(theptr));
				cs->generateDecompositionBindings(this->mappings, res, !(_array->getType()->isRangeType() || _array->getType()->isStringType()));

				if(this->indexVar)
				{
					auto idx = util::pool<RawValueExpr>(this->indexVar->loc, fir::Type::getNativeWord());
					idx->rawValue = CGResult(cs->irb.ReadPtr(iterptr));

					this->indexVar->init = idx;
					this->indexVar->codegen(cs);
				}
			};

			this->body->codegen(cs);
		}
		cs->leaveBreakableBody();


		// increment the index
		cs->irb.WritePtr(cs->irb.Add(cs->irb.ReadPtr(idxptr), step), idxptr);
		cs->irb.WritePtr(cs->irb.Add(cs->irb.ReadPtr(iterptr), fir::ConstantInt::getNative(1)), iterptr);

		cs->irb.UnCondBranch(check);
	}

	cs->irb.setCurrentBlock(merge);

	return CGResult(0);
}

std::vector<sst::Block*> sst::ForeachLoop::getBlocks()
{
	return { this->body };
}























