// loops.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "sst.h"
#include "codegen.h"

CGResult sst::WhileLoop::_codegen(cgn::CodegenState* cs, fir::Type* inferred)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	auto loop = cs->irb.addNewBlockAfter("loop", cs->irb.getCurrentBlock());
	auto merge = cs->irb.addNewBlockAfter("merge", cs->irb.getCurrentBlock());


	auto getcond = [](cgn::CodegenState* cs, Expr* c) -> fir::Value* {

		auto cv = cs->oneWayAutocast(c->codegen(cs, fir::Type::getBool()), fir::Type::getBool());
		if(cv.value->getType() != fir::Type::getBool())
			error(c, "Non-boolean expression with type '%s' cannot be used as a conditional", cv.value->getType());

		// ok
		return cs->irb.ICmpEQ(cv.value, fir::ConstantBool::get(true));
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

		// ok, check if we have a condition
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
		auto check = cs->irb.addNewBlockAfter("check", cs->irb.getCurrentBlock());
		cs->irb.UnCondBranch(check);
		cs->irb.setCurrentBlock(check);

		// ok
		iceAssert(this->cond);
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
	auto prevblock = cs->irb.getCurrentBlock();

	auto check = cs->irb.addNewBlockAfter("check", cs->irb.getCurrentBlock());
	auto loop = cs->irb.addNewBlockAfter("loop", cs->irb.getCurrentBlock());
	auto merge = cs->irb.addNewBlockAfter("merge", cs->irb.getCurrentBlock());

	fir::Value* end = 0;
	fir::Value* step = 0;

	fir::Value* idxptr = cs->irb.StackAlloc(fir::Type::getInt64());
	fir::Value* iterptr = cs->irb.StackAlloc(fir::Type::getInt64());

	auto [ array, arrayptr, _ ] = this->array->codegen(cs);
	if(array->getType()->isRangeType())
	{
		cs->irb.Store(cs->irb.GetRangeLower(array), idxptr);
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

		end = cs->irb.Add(end, cs->irb.Select(cs->irb.ICmpGEQ(step, fir::ConstantInt::getInt64(0)),
			fir::ConstantInt::getInt64(1), fir::ConstantInt::getInt64(-1)));
	}
	else
	{
		cs->irb.Store(fir::ConstantInt::getInt64(0), idxptr);
		step = fir::ConstantInt::getInt64(1);

		if(array->getType()->isDynamicArrayType())
		{
			end = cs->irb.GetDynamicArrayLength(array);
		}
		else if(array->getType()->isArraySliceType())
		{
			end = cs->irb.GetArraySliceLength(array);
		}
		else if(array->getType()->isStringType())
		{
			end = cs->irb.GetStringLength(array);
		}
		else if(array->getType()->isArrayType())
		{
			end = fir::ConstantInt::getInt64(array->getType()->toArrayType()->getArraySize());
			iceAssert(arrayptr);
		}
		else
		{
			error(this->array, "Unsupported type '%s' in foreach loop", array->getType());
		}
	}

	cs->irb.UnCondBranch(check);
	cs->irb.setCurrentBlock(check);

	//! here's some special shit where we handle ranges with start > end
	fir::Value* cond = 0;
	if(array->getType()->isRangeType())
	{
		cond = cs->irb.Select(cs->irb.ICmpGT(step, fir::ConstantInt::getInt64(0)),
			cs->irb.ICmpLT(cs->irb.Load(idxptr), end),		// i < end for step > 0
			cs->irb.ICmpGT(cs->irb.Load(idxptr), end));		// i > end for step < 0
	}
	else
	{
		cond = cs->irb.ICmpLT(cs->irb.Load(idxptr), end);
	}

	iceAssert(cond);
	cs->irb.CondBranch(cond, loop, merge);

	cs->irb.setCurrentBlock(loop);
	{
		fir::Value* theptr = 0;
		if(array->getType()->isRangeType())
			theptr = idxptr;

		else if(array->getType()->isDynamicArrayType())
			theptr = cs->irb.PointerAdd(cs->irb.GetDynamicArrayData(array), cs->irb.Load(idxptr));

		else if(array->getType()->isArraySliceType())
			theptr = cs->irb.PointerAdd(cs->irb.GetArraySliceData(array), cs->irb.Load(idxptr));

		else if(array->getType()->isStringType())
			theptr = cs->irb.PointerTypeCast(cs->irb.PointerAdd(cs->irb.GetStringData(array), cs->irb.Load(idxptr)), fir::Type::getChar()->getPointerTo());

		else if(array->getType()->isArrayType())
			theptr = cs->irb.PointerAdd(cs->irb.ConstGEP2(arrayptr, 0, 0), cs->irb.Load(idxptr));

		else
			iceAssert(0);

		auto res = CGResult(cs->irb.Load(theptr), theptr);
		cs->generateDecompositionBindings(this->mappings, res, !(array->getType()->isRangeType() || array->getType()->isStringType()));

		if(this->indexVar)
		{
			auto idx = new sst::RawValueExpr(this->indexVar->loc, fir::Type::getInt64());
			idx->rawValue = CGResult(cs->irb.Load(iterptr));

			this->indexVar->init = idx;
			this->indexVar->codegen(cs);

			if(cs->isRefCountedType(res.value->getType()))
				cs->addRefCountedValue(res.value);
		}


		// make the block
		cs->enterBreakableBody(cgn::ControlFlowPoint(this->body, merge, check));
		{
			this->body->codegen(cs);
		}
		cs->leaveBreakableBody();


		// increment the index
		cs->irb.Store(cs->irb.Add(cs->irb.Load(idxptr), step), idxptr);
		cs->irb.Store(cs->irb.Add(cs->irb.Load(iterptr), fir::ConstantInt::getInt64(1)), iterptr);

		cs->irb.UnCondBranch(check);
	}

	cs->irb.setCurrentBlock(merge);

	return CGResult(0);
}

std::vector<sst::Block*> sst::ForeachLoop::getBlocks()
{
	return { this->body };
}























