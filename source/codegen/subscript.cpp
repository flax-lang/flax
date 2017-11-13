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

	// assists checking for literal writes later on
	this->cgSubscripteePtr = lr.pointer;
	this->cgSubscriptee = lr.value;
	this->cgIndex = index;

	fir::Value* data = 0;
	if(lt->isDynamicArrayType() || lt->isArraySliceType() || lt->isArrayType())
	{
		// ok, do the thing
		auto checkf = cgn::glue::array::getBoundsCheckFunction(cs, false);
		iceAssert(checkf);
		iceAssert(lr.pointer);

		fir::Value* max = 0;
		if(lt->isDynamicArrayType())	max = cs->irb.CreateGetDynamicArrayLength(lr.value);
		else if(lt->isArraySliceType())	max = cs->irb.CreateGetArraySliceLength(lr.value);
		else if(lt->isArrayType())		max = fir::ConstantInt::getInt64(lt->toArrayType()->getArraySize());

		auto ind = index;
		auto locstr = fir::ConstantString::get(this->loc.toString());

		// call it
		cs->irb.CreateCall3(checkf, max, ind, locstr);

		// ok.
		if(lt->isArrayType())
		{
			// do a manual thing, return here immediately.
			auto ret = cs->irb.CreateGEP2(lr.pointer, fir::ConstantInt::getInt64(0), ind);
			return CGResult(cs->irb.CreateLoad(ret), ret);
		}
		else if(lt->isDynamicArrayType())
		{
			data = cs->irb.CreateGetDynamicArrayData(lr.value);
		}
		else if(lt->isArraySliceType())
		{
			data = cs->irb.CreateGetArraySliceData(lr.value);
		}

		if(lr.pointer->isImmutable())
			data->makeImmutable();
	}
	else if(lt->isStringType())
	{
		// bounds check
		auto checkf = cgn::glue::string::getBoundsCheckFunction(cs);
		iceAssert(checkf);

		auto locstr = fir::ConstantString::get(this->loc.toString());

		// call it
		cs->irb.CreateCall3(checkf, lr.value, index, locstr);
		data = cs->irb.CreateGetStringData(lr.value);
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



















