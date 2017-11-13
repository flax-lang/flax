// builtin.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "errors.h"
#include "codegen.h"
#include "typecheck.h"

#define dcast(t, v)		dynamic_cast<t*>(v)

CGResult sst::BuiltinDotOp::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	auto res = this->lhs->codegen(cs);
	auto ty = res.value->getType();

	if(ty->isStringType())
	{
		if(this->name == "length")
		{
			return CGResult(cs->irb.CreateGetStringLength(res.value));
		}
		if(this->name == "count")
		{
			auto fn = cgn::glue::string::getUnicodeLengthFunction(cs);
			iceAssert(fn);

			auto ret = cs->irb.CreateCall1(fn, cs->irb.CreateGetStringData(res.value));
			return CGResult(ret);
		}
		else if(this->name == "ptr")
		{
			return CGResult(cs->irb.CreateGetStringData(res.value));
		}
		else if(this->name == "rc")
		{
			return CGResult(cs->irb.CreateGetStringRefCount(res.value));
		}
	}
	else if(ty->isDynamicArrayType())
	{
		if(this->name == "length" || this->name == "count")
		{
			return CGResult(cs->irb.CreateGetDynamicArrayLength(res.value));
		}
		if(this->name == "capacity")
		{
			return CGResult(cs->irb.CreateGetDynamicArrayCapacity(res.value));
		}
		else if(this->name == "ptr")
		{
			return CGResult(cs->irb.CreateGetDynamicArrayData(res.value));
		}
		else if(this->name == "rc")
		{
			return CGResult(cs->irb.CreateGetDynamicArrayRefCount(res.value));
		}
	}
	else if(ty->isArraySliceType())
	{
		if(this->name == "length" || this->name == "count")
		{
			return CGResult(cs->irb.CreateGetArraySliceLength(res.value));
		}
		else if(this->name == "ptr")
		{
			return CGResult(cs->irb.CreateGetArraySliceData(res.value));
		}
	}
	else if(ty->isArrayType())
	{
		if(this->name == "length" || this->name == "count")
		{
			CGResult(fir::ConstantInt::getInt64(ty->toArrayType()->getArraySize()));
		}
		else if(this->name == "ptr")
		{
			iceAssert(res.pointer);
			auto ret = cs->irb.CreateGEP2(res.pointer, fir::ConstantInt::getInt64(0), fir::ConstantInt::getInt64(0));
			return CGResult(ret);
		}
	}


	error(this, "No such property '%s' on type '%s'", this->name, ty->str());
}






