// subscript.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "sst.h"
#include "codegen.h"

#define dcast(t, v)		dynamic_cast<t*>(v)

CGResult sst::SubscriptOp::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	// first gen the inside
	fir::Value* index = this->inside->codegen(cs).value;
	iceAssert(index->getType()->isIntegerType() || index->getType()->isConstantNumberType());
	if(index->getType()->isConstantNumberType())
	{
		auto cv = dcast(fir::ConstantValue, index);
		iceAssert(cv);

		index = cs->unwrapConstantNumber(cv);
	}

	// of course these will have to be changed eventually
	iceAssert(index);
	iceAssert(index->getType()->isIntegerType());

	// check what's the left
	auto lr = this->expr->codegen(cs);
	auto lt = lr.value->getType();

	fir::Value* data = 0;
	if(lt->isDynamicArrayType() || lt->isArraySliceType())
	{
		// ok, do the thing
		auto checkf = cgn::glue::array::getBoundsCheckFunction(cs, false);
		iceAssert(checkf);
		iceAssert(lr.pointer);

		auto max = (lt->isDynamicArrayType() ? cs->irb.CreateGetDynamicArrayLength(lr.pointer)
			: cs->irb.CreateGetArraySliceLength(lr.pointer));

		auto ind = index;
		auto str = fir::ConstantString::get(this->loc.toString());

		// call it
		cs->irb.CreateCall3(checkf, max, ind, str);

		// ok.
		data = cs->irb.CreateGetDynamicArrayData(lr.pointer);
	}
	else if(lt->isPointerType())
	{
		data = lr.value;
	}
	else
	{
		iceAssert(0 && "how?");
	}


	// ok, do it
	fir::Value* ptr = cs->irb.CreateGetPointer(data, index);
	fir::Value* val = cs->irb.CreateLoad(ptr);

	return CGResult(val, ptr, CGResult::VK::LValue);
}



















