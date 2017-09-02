// call.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "sst.h"
#include "codegen.h"

#define dcast(t, v)		dynamic_cast<t*>(v)

CGResult sst::FunctionCall::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	if(!this->target)
		error(this, "Failed to find target for function call to '%s'", this->name.c_str());

	auto vf = this->target->codegen(cs).value;
	fir::FunctionType* ft = 0;

	if(vf->getType()->isFunctionType())
	{
		ft = vf->getType()->toFunctionType();
	}
	else
	{
		auto vt = vf->getType();
		iceAssert(vt->isPointerType() && vt->getPointerElementType()->isFunctionType());

		ft = vt->getPointerElementType()->toFunctionType();
	}

	iceAssert(ft);

	// auto fn = dynamic_cast<fir::Function*>(vf);
	// iceAssert(fn);

	size_t numArgs = ft->getArgumentTypes().size();
	if(!ft->isCStyleVarArg() && this->arguments.size() != numArgs)
	{
		error(this, "Mismatch in number of arguments in call to '%s'; %zu %s provided, but %zu %s expected",
			this->name.c_str(), this->arguments.size(), this->arguments.size() == 1 ? "was" : "were", numArgs,
			numArgs == 1 ? "was" : "were");
	}
	else if(ft->isCStyleVarArg() && this->arguments.size() < numArgs)
	{
		error(this, "Need at least %zu arguments to call variadic function '%s', only have %zu",
			numArgs, this->name.c_str(), this->arguments.size());
	}


	size_t i = 0;
	std::vector<fir::Value*> args;
	for(auto arg : this->arguments)
	{
		fir::Type* inf = 0;
		if(i < numArgs)
			inf = ft->getArgumentN(i);

		auto val = arg->codegen(cs, inf).value;
		if(val->getType()->isConstantNumberType())
		{
			auto cv = dcast(fir::ConstantValue, val);
			iceAssert(cv);

			val = cs->unwrapConstantNumber(cv);
			if(auto cf = dcast(fir::ConstantFP, val))
				warn(arg, "%f", cf->getValue());
		}

		if(i < numArgs && val->getType() != ft->getArgumentN(i))
		{
			error(arg, "Mismatched type in function call; parameter has type '%s', but given argument has type '%s'",
				ft->getArgumentN(i)->str().c_str(), val->getType()->str().c_str());
		}

		args.push_back(val);
		i++;
	}

	if(fir::Function* func = dcast(fir::Function, vf))
	{
		return CGResult(cs->irb.CreateCall(func, args));
	}
	else
	{
		iceAssert(vf->getType()->getPointerElementType()->isFunctionType());
		auto fptr = cs->irb.CreateLoad(vf);

		return CGResult(cs->irb.CreateCallToFunctionPointer(fptr, ft, args));
	}
}

















