// loops.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "sst.h"
#include "codegen.h"

#define dcast(t, v)		dynamic_cast<t*>(v)

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


CGResult sst::ForeachLoop::_codegen(cgn::CodegenState* cs, fir::Type* inferred)
{
	cs->pushLoc(this->loc);
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

	auto loop = cs->irb.addNewBlockAfter("loop", cs->irb.getCurrentBlock());
	auto check = cs->irb.addNewBlockAfter("check", cs->irb.getCurrentBlock());
	auto merge = cs->irb.addNewBlockAfter("merge", cs->irb.getCurrentBlock());

	fir::Value* end = 0;
	fir::Value* step = 0;
	fir::Value* idxptr = cs->irb.StackAlloc(fir::Type::getInt64());

	fir::Value* data = 0;

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

			* TODO: same kinda problem as the range, adding 1 might screw up when the end is less than the beginning, or when we go into negative things.
		*/

		end = cs->irb.Add(end, fir::ConstantInt::getInt64(1));
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

	fir::Value* cond = cs->irb.ICmpLT(cs->irb.Load(idxptr), end);
	cs->irb.CondBranch(cond, loop, merge);

	cs->irb.setCurrentBlock(loop);
	{
		// codegen the thing.
		auto ptr = this->var->codegen(cs).pointer;
		iceAssert(ptr);

		// get the data.
		fir::Value* val = 0;
		if(array->getType()->isRangeType())
			val = cs->irb.Load(idxptr);

		else if(array->getType()->isDynamicArrayType())
			val = cs->irb.Load(cs->irb.PointerAdd(cs->irb.GetDynamicArrayData(array), cs->irb.Load(idxptr)));

		else if(array->getType()->isArraySliceType())
			val = cs->irb.Load(cs->irb.PointerAdd(cs->irb.GetArraySliceData(array), cs->irb.Load(idxptr)));

		else if(array->getType()->isStringType())
			val = cs->irb.Bitcast(cs->irb.Load(cs->irb.PointerAdd(cs->irb.GetStringData(array), cs->irb.Load(idxptr))), fir::Type::getChar());

		else if(array->getType()->isArrayType())
			val = cs->irb.Load(cs->irb.PointerAdd(cs->irb.ConstGEP2(arrayptr, 0, 0), cs->irb.Load(idxptr)));

		else
			iceAssert(0);

		ptr->makeNotImmutable();
		cs->irb.Store(val, ptr);
		ptr->makeImmutable();


		// make the block
		cs->enterBreakableBody(cgn::ControlFlowPoint(this->body, merge, check));
		{
			this->body->codegen(cs);
		}
		cs->leaveBreakableBody();


		// increment the index
		cs->irb.Store(cs->irb.Add(cs->irb.Load(idxptr), step), idxptr);
		cs->irb.UnCondBranch(check);
	}

	cs->irb.setCurrentBlock(merge);

	return CGResult(0);
}

























