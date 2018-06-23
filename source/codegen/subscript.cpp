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

	// check what's the left
	auto lr = this->expr->codegen(cs);
	auto lt = lr.value->getType();

	fir::Function* boundscheckfn = cgn::glue::saa_common::generateBoundsCheckFunction(cs,
		/* isString: */ lt->isStringType(), /* isDecomp: */false);;

	fir::Value* datapointer = 0;
	fir::Value* maxlength = 0;

	if(lt->isStringType() || lt->isDynamicArrayType())
	{
		datapointer = cs->irb.GetSAAData(lr.value);
		maxlength = cs->irb.GetSAALength(lr.value);
	}
	else if(lt->isArraySliceType())
	{
		datapointer = cs->irb.GetArraySliceData(lr.value);
		maxlength = cs->irb.GetArraySliceLength(lr.value);
	}
	else if(lt->isArrayType())
	{
		// TODO: LVALUE HOLE
		if(lr->islorclvalue())
		{
			datapointer = cs->irb.GEP2(cs->irb.AddressOf(lr.value, true), fir::ConstantInt::getInt64(0),
				fir::ConstantInt::getInt64(0));
			maxlength = fir::ConstantInt::getInt64(lt->toArrayType()->getArraySize());
		}
		else
		{
			error("NOT SUP");
		}
	}
	else if(lt->isPointerType())
	{
		datapointer = lr.value;
		maxlength = 0;
	}
	else
	{
		iceAssert(0 && "how?");
	}


	// first gen the inside
	fir::Value* index = this->inside->codegen(cs).value;
	{
		if(index->getType()->isConstantNumberType())
		{
			auto cv = dcast(fir::ConstantValue, index);
			iceAssert(cv);

			index = cs->unwrapConstantNumber(cv);
		}

		// of course these will have to be changed eventually
		iceAssert(index->getType()->isIntegerType());
	}

	if(maxlength)
		cs->irb.Call(boundscheckfn, maxlength, index, fir::ConstantString::get(this->loc.shortString()));

	// ok, do it
	fir::Value* ptr = cs->irb.GetPointer(datapointer, index);
	return CGResult(cs->irb.Dereference(ptr));
}



CGResult sst::SubscriptDollarOp::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	return CGResult(cs->getCurrentSubscriptArrayLength());
}

















