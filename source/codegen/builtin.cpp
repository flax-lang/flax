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

	error(this, "No such property '%s' on type '%s'", this->name, ty->str());
}
