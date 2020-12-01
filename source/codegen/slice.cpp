// slice.cpp
// Copyright (c) 2014 - 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include "sst.h"
#include "codegen.h"
#include "platform.h"
#include "gluecode.h"
#include "memorypool.h"

static void checkSliceOperation(cgn::CodegenState* cs, sst::Expr* user, fir::Value* maxlen, fir::Value* beginIndex, fir::Value* endIndex,
	sst::Expr* bexpr, sst::Expr* eexpr)
{
	Location apos = (bexpr ? bexpr->loc : user->loc);
	Location bpos = (eexpr ? eexpr->loc : user->loc);

	iceAssert(beginIndex);
	iceAssert(endIndex);

	if(!beginIndex->getType()->isIntegerType())
		error(bexpr, "expected integer type for array slice; got '%s'", beginIndex->getType());

	if(!endIndex->getType()->isIntegerType())
		error(eexpr, "expected integer type for array slice; got '%s'", endIndex->getType());


	// do a check
	auto neg_begin = cs->irb.addNewBlockInFunction("neg_begin", cs->irb.getCurrentFunction());
	auto neg_end = cs->irb.addNewBlockInFunction("neg_end", cs->irb.getCurrentFunction());
	auto neg_len = cs->irb.addNewBlockInFunction("neg_len", cs->irb.getCurrentFunction());
	auto check1 = cs->irb.addNewBlockInFunction("check1", cs->irb.getCurrentFunction());
	auto check2 = cs->irb.addNewBlockInFunction("check2", cs->irb.getCurrentFunction());
	auto merge = cs->irb.addNewBlockInFunction("merge", cs->irb.getCurrentFunction());

	{
		fir::Value* neg = cs->irb.ICmpLT(beginIndex, fir::ConstantInt::getNative(0));
		cs->irb.CondBranch(neg, neg_begin, check1);
	}

	cs->irb.setCurrentBlock(check1);
	{
		fir::Value* neg = cs->irb.ICmpLT(endIndex, fir::ConstantInt::getNative(0));
		cs->irb.CondBranch(neg, neg_end, check2);
	}

	cs->irb.setCurrentBlock(check2);
	{
		fir::Value* neg = cs->irb.ICmpLT(endIndex, beginIndex);
		cs->irb.CondBranch(neg, neg_len, merge);
	}


	cs->irb.setCurrentBlock(neg_begin);
	cgn::glue::printRuntimeError(cs, fir::ConstantCharSlice::get(apos.toString()),
		"slice start index '%ld' is < 0\n", { beginIndex });

	cs->irb.setCurrentBlock(neg_end);
	cgn::glue::printRuntimeError(cs, fir::ConstantCharSlice::get(bpos.toString()),
		"slice end index '%ld' is < 0\n", { endIndex });

	cs->irb.setCurrentBlock(neg_len);
	cgn::glue::printRuntimeError(cs, fir::ConstantCharSlice::get(bpos.toString()),
		"slice end index '%ld' is < start index '%ld'\n", { endIndex, beginIndex });


	cs->irb.setCurrentBlock(merge);

	// bounds check.
	{
		// endindex is non-inclusive -- if we're doing a decomposition check then it compares length instead
		// of indices here.
		fir::Function* checkf = cgn::glue::array::getBoundsCheckFunction(cs, /* isPerformingDecomposition: */ true);
		if(checkf)
			cs->irb.Call(checkf, maxlen, endIndex, fir::ConstantCharSlice::get(apos.toString()));
	}
}




static CGResult performSliceOperation(cgn::CodegenState* cs, sst::SliceOp* user, bool check, fir::Type* elmType, fir::Value* data,
	fir::Value* maxlen, fir::Value* beginIndex, fir::Value* endIndex, sst::Expr* bexpr, sst::Expr* eexpr)
{
	if(check)
		checkSliceOperation(cs, user, maxlen, beginIndex, endIndex, bexpr, eexpr);

	// ok, make the slice
	fir::Type* slct = user->type;
	iceAssert(slct->isArraySliceType());

	fir::Value* slice = cs->irb.CreateValue(slct, "slice");

	// FINALLY.
	// increment ptr
	fir::Value* newptr = cs->irb.GetPointer(data, beginIndex, "newptr");
	fir::Value* newlen = cs->irb.Subtract(endIndex, beginIndex, "newlen");

	slice = cs->irb.SetArraySliceData(slice, newptr);
	slice = cs->irb.SetArraySliceLength(slice, newlen);

	// slices are rvalues
	return CGResult(slice);
}





CGResult sst::SliceOp::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	auto ty = this->expr->type;
	auto res = this->expr->codegen(cs);

	auto lhs = res.value;

	iceAssert(ty == lhs->getType());

	fir::Value* length = 0;
	if(ty->isDynamicArrayType())	length = cs->irb.GetSAALength(lhs, "orig_len");
	else if(ty->isArraySliceType())	length = cs->irb.GetArraySliceLength(lhs, "orig_len");
	else if(ty->isStringType())		length = cs->irb.GetSAALength(lhs, "orig_len");
	else if(ty->isArrayType())		length = fir::ConstantInt::getNative(ty->toArrayType()->getArraySize());
	else if(ty->isPointerType())    length = fir::ConstantInt::getNative(0);
	else							error(this, "unsupported type '%s'", ty);

	fir::Value* beginIdx = 0;
	fir::Value* endIdx = 0;

	iceAssert(length);
	{
		// add the dollar value.
		cs->enterSubscriptWithLength(length);
		defer(cs->leaveSubscript());

		if(ty->isPointerType() && !this->end)
			error(this, "slicing operation on pointers requires an ending index");

		if(this->begin)	beginIdx = this->begin->codegen(cs).value;
		else			beginIdx = fir::ConstantInt::getNative(0);

		if(this->end)	endIdx = this->end->codegen(cs).value;
		else			endIdx = length;

		beginIdx = cs->oneWayAutocast(beginIdx, fir::Type::getNativeWord());
		endIdx = cs->oneWayAutocast(endIdx, fir::Type::getNativeWord());
	}

	beginIdx->setName("begin");
	endIdx->setName("end");

	/*
		as a reminder:

		performSliceOperation(  cgn::CodegenState* cs,
								sst::Expr* user,
								bool check,
								fir::Type* elmType,
								fir::Value* data,
								fir::Value* maxlen,
								fir::Value* beginIndex,
								fir::Value* endIndex,
								sst::Expr* bexpr,
								sst::Expr* eexpr)
	 */

	//* note: mutability determination is done at the typechecking phase.
	if(ty->isPointerType())
	{
		return performSliceOperation(cs, this, false, ty->getPointerElementType(), lhs,
			length, beginIdx, endIdx, this->begin, this->end);
	}
	else if(ty->isDynamicArrayType())
	{
		return performSliceOperation(cs, this, true, ty->getArrayElementType(), cs->irb.GetSAAData(lhs),
			length, beginIdx, endIdx, this->begin, this->end);
	}
	else if(ty->isArrayType())
	{
		// TODO: LVALUE HOLE
		fir::Value* lhsptr = 0;

		if(!lhsptr) lhsptr = cs->irb.ImmutStackAlloc(lhs->getType(), lhs);
		iceAssert(lhsptr);

		fir::Value* data = cs->irb.ConstGEP2(lhsptr, 0, 0);

		return performSliceOperation(cs, this, true, ty->getArrayElementType(), data,
			length, beginIdx, endIdx, this->begin, this->end);
	}
	else if(ty->isArraySliceType())
	{
		return performSliceOperation(cs, this, true, ty->getArrayElementType(), cs->irb.GetArraySliceData(lhs),
			length, beginIdx, endIdx, this->begin, this->end);
	}
	else if(ty->isStringType())
	{
		return performSliceOperation(cs, this, true, fir::Type::getInt8(), cs->irb.GetSAAData(lhs),
			length, beginIdx, endIdx, this->begin, this->end);
	}
	else
	{
		error(this, "cannot slice unsupported type '%s'", ty);
	}
}







CGResult sst::SplatExpr::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	// right. what we want to do is to see the kind of shit that we have.
	auto ty = this->inside->type;

	if(ty->isArraySliceType())
	{
		if(ty->isVariadicArrayType())
		{
			auto ret = this->inside->codegen(cs, infer);
			iceAssert(ret->getType()->isVariadicArrayType());

			return ret;
		}
		else
		{
			auto ret = this->inside->codegen(cs, infer);
			iceAssert(ret->getType()->isArraySliceType());

			return CGResult(cs->irb.Bitcast(ret.value, fir::ArraySliceType::getVariadic(ret->getType()->getArrayElementType())));
		}
	}
	else if(ty->isDynamicArrayType() || ty->isArrayType())
	{
		// just do a slice on it.
		auto target = fir::ArraySliceType::getVariadic(ty->getArrayElementType());
		auto slice = util::pool<SliceOp>(this->loc, target);

		slice->expr = this->inside;
		slice->begin = 0;
		slice->end = 0;

		auto ret = slice->codegen(cs, infer);
		return CGResult(cs->irb.Bitcast(ret.value, target));
	}
	else
	{
		error(this, "unexpected type '%s' in splat expression", ty);
	}
}

































