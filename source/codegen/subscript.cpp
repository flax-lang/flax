// subscript.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "sst.h"
#include "codegen.h"
#include "gluecode.h"

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
	// this->cgSubscripteePtr = lr.pointer;

	this->cgSubscriptee = lr.value;
	this->cgIndex = index;

	fir::Value* data = 0;
	if(lt->isDynamicArrayType() || lt->isArraySliceType() || lt->isArrayType())
	{
		// ok, do the thing
		auto checkf = cgn::glue::array::getBoundsCheckFunction(cs, false);
		iceAssert(checkf);

		fir::Value* max = 0;
		if(lt->isDynamicArrayType())	max = cs->irb.GetSAALength(lr.value);
		else if(lt->isArraySliceType())	max = cs->irb.GetArraySliceLength(lr.value);
		else if(lt->isArrayType())		max = fir::ConstantInt::getInt64(lt->toArrayType()->getArraySize());

		auto ind = index;
		auto locstr = fir::ConstantString::get(this->loc.toString());

		// call it
		cs->irb.Call(checkf, max, ind, locstr);

		// ok.
		if(lt->isArrayType())
		{
			// TODO: LVALUE HOLE
			if(lr->islorclvalue())
			{
				return CGResult(cs->irb.GEP2(lr.value, fir::ConstantInt::getInt64(0), ind));
			}
			else
			{
				// return CGResult(cs->irb.ExtractValue(lr.value, { ind }));
				error("NOT SUP");
			}
		}
		else if(lt->isDynamicArrayType())
		{
			data = cs->irb.GetSAAData(lr.value);
		}
		else if(lt->isArraySliceType())
		{
			data = cs->irb.GetArraySliceData(lr.value);
		}
	}
	else if(lt->isStringType())
	{
		// bounds check
		auto checkf = cgn::glue::string::getBoundsCheckFunction(cs, false);
		iceAssert(checkf);

		auto locstr = fir::ConstantString::get(this->loc.toString());

		// call it
		cs->irb.Call(checkf, cs->irb.GetSAALength(lr.value), index, locstr);
		data = cs->irb.GetSAAData(lr.value);
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
	fir::Value* ptr = cs->irb.GetPointer(data, index);
	// fir::Value* val = cs->irb.ReadPtr(ptr);

	return CGResult(cs->irb.Dereference(ptr));
}



















