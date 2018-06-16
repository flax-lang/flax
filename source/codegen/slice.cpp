// slice.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "sst.h"
#include "codegen.h"
#include "platform.h"
#include "gluecode.h"

static void checkSliceOperation(cgn::CodegenState* cs, sst::Expr* user, fir::Value* maxlen, fir::Value* beginIndex, fir::Value* endIndex,
	sst::Expr* bexpr, sst::Expr* eexpr)
{
	Location apos = (bexpr ? bexpr->loc : user->loc);
	Location bpos = (eexpr ? eexpr->loc : user->loc);

	iceAssert(beginIndex);
	iceAssert(endIndex);

	if(!beginIndex->getType()->isIntegerType())
		error(bexpr, "Expected integer type for array slice; got '%s'", beginIndex->getType());

	if(!endIndex->getType()->isIntegerType())
		error(eexpr, "Expected integer type for array slice; got '%s'", endIndex->getType());


	fir::Value* length = cs->irb.Subtract(endIndex, beginIndex);

	// do a check
	auto neg_begin = cs->irb.addNewBlockInFunction("neg_begin", cs->irb.getCurrentFunction());
	auto neg_end = cs->irb.addNewBlockInFunction("neg_end", cs->irb.getCurrentFunction());
	auto neg_len = cs->irb.addNewBlockInFunction("neg_len", cs->irb.getCurrentFunction());
	auto check1 = cs->irb.addNewBlockInFunction("check1", cs->irb.getCurrentFunction());
	auto check2 = cs->irb.addNewBlockInFunction("check2", cs->irb.getCurrentFunction());
	auto merge = cs->irb.addNewBlockInFunction("merge", cs->irb.getCurrentFunction());

	{
		fir::Value* neg = cs->irb.ICmpLT(beginIndex, fir::ConstantInt::getInt64(0));
		cs->irb.CondBranch(neg, neg_begin, check1);
	}

	cs->irb.setCurrentBlock(check1);
	{
		fir::Value* neg = cs->irb.ICmpLT(endIndex, fir::ConstantInt::getInt64(0));
		cs->irb.CondBranch(neg, neg_end, check2);
	}

	cs->irb.setCurrentBlock(check2);
	{
		fir::Value* neg = cs->irb.ICmpLT(length, fir::ConstantInt::getInt64(0));
		cs->irb.CondBranch(neg, neg_len, merge);
	}


	cs->irb.setCurrentBlock(neg_begin);
	cgn::glue::printRuntimeError(cs, fir::ConstantString::get(apos.toString()), "Start index of array slice was negative (got '%ld')\n", { beginIndex });

	cs->irb.setCurrentBlock(neg_end);
	cgn::glue::printRuntimeError(cs, fir::ConstantString::get(bpos.toString()), "End index of array slice was negative (got '%ld')\n", { endIndex });

	cs->irb.setCurrentBlock(neg_len);
	cgn::glue::printRuntimeError(cs, fir::ConstantString::get(bpos.toString()), "Length of array slice was negative (got '%ld')\n", { length });


	cs->irb.setCurrentBlock(merge);

	// bounds check.
	{
		// endindex is non-inclusive, so do the len vs len check
		fir::Function* checkf = cgn::glue::array::getBoundsCheckFunction(cs, true);
		iceAssert(checkf);

		cs->irb.Call(checkf, maxlen, endIndex, fir::ConstantString::get(apos.toString()));
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
	fir::Value* newptr = cs->irb.PointerAdd(data, beginIndex, "newptr");
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

	// this->cgSubscripteePtr = res.pointer;
	this->cgSubscriptee = res.value;

	auto lhs = res.value;
	// auto lhsptr = res.pointer;

	iceAssert(ty == lhs->getType());

	fir::Value* length = 0;
	if(ty->isDynamicArrayType())	length = cs->irb.GetSAALength(lhs, "orig_len");
	else if(ty->isArraySliceType())	length = cs->irb.GetArraySliceLength(lhs, "orig_len");
	else if(ty->isStringType())		length = cs->irb.GetSAALength(lhs, "orig_len");
	else if(ty->isArrayType())		length = fir::ConstantInt::getInt64(ty->toArrayType()->getArraySize());
	else if(ty->isPointerType())    length = fir::ConstantInt::getInt64(0);
	else							error(this, "unsupported type '%s'", ty);

	iceAssert(length);
	{
		if(ty->isPointerType() && !this->end)
			error(this, "Slicing operation on pointers requires an ending index");

		if(this->begin)	this->cgBegin = this->begin->codegen(cs).value;
		else			this->cgBegin = fir::ConstantInt::getInt64(0);

		if(this->end)	this->cgEnd = this->end->codegen(cs).value;
		else			this->cgEnd = length;

		this->cgBegin = cs->oneWayAutocast(CGResult(this->cgBegin), fir::Type::getInt64()).value;
		this->cgEnd = cs->oneWayAutocast(CGResult(this->cgEnd), fir::Type::getInt64()).value;
	}

	this->cgBegin->setName("begin");
	this->cgEnd->setName("end");

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
			length, this->cgBegin, this->cgEnd, this->begin, this->end);
	}
	else if(ty->isDynamicArrayType())
	{
		return performSliceOperation(cs, this, true, ty->getArrayElementType(), cs->irb.GetSAAData(lhs),
			length, this->cgBegin, this->cgEnd, this->begin, this->end);
	}
	else if(ty->isArrayType())
	{
		// TODO: LVALUE HOLE
		fir::Value* lhsptr = 0;

		if(!lhsptr) lhsptr = cs->irb.ImmutStackAlloc(lhs->getType(), lhs);
		iceAssert(lhsptr);

		fir::Value* data = cs->irb.ConstGEP2(lhsptr, 0, 0);

		return performSliceOperation(cs, this, true, ty->getArrayElementType(), data,
			length, this->cgBegin, this->cgEnd, this->begin, this->end);
	}
	else if(ty->isArraySliceType())
	{
		return performSliceOperation(cs, this, true, ty->getArrayElementType(), cs->irb.GetArraySliceData(lhs),
			length, this->cgBegin, this->cgEnd, this->begin, this->end);
	}
	else if(ty->isStringType())
	{
		return performSliceOperation(cs, this, true, fir::Type::getInt8(), cs->irb.GetSAAData(lhs),
			length, this->cgBegin, this->cgEnd, this->begin, this->end);
	}
	else
	{
		error(this, "Cannot slice unsupported type '%s'", ty);
	}
}


