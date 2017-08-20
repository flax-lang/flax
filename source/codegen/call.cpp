// call.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "sst.h"
#include "codegen.h"


CGResult sst::FunctionCall::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	if(!this->target)
		error(this, "Failed to find target for function call to '%s'", this->name.c_str());

	auto fn = dynamic_cast<fir::Function*>(this->target->codegen(cs).value);
	iceAssert(fn);

	if(!fn->isCStyleVarArg() && this->arguments.size() != fn->getArgumentCount())
	{
		error(this, "Mismatch in number of arguments in call to '%s'; %zu %s provided, but %zu %s expected",
			this->name.c_str(), this->arguments.size(), this->arguments.size() == 1 ? "was" : "were", fn->getArgumentCount(),
			fn->getArgumentCount() == 1 ? "was" : "were");
	}
	else if(fn->isCStyleVarArg() && this->arguments.size() < fn->getArgumentCount())
	{
		error(this, "Need at least %zu arguments to call variadic function '%s', only have %zu",
			fn->getArgumentCount(), this->name.c_str(), this->arguments.size());
	}

	size_t i = 0;
	std::vector<fir::Value*> args;
	for(auto arg : this->arguments)
	{
		fir::Type* inf = 0;
		if(i < fn->getArgumentCount())
			inf = fn->getArguments()[i]->getType();

		auto val = arg->codegen(cs, inf).value;

		if(i < fn->getArgumentCount() && val->getType() != fn->getArguments()[i]->getType())
		{
			error(arg, "Mismatched type in function call; parameter has type '%s', but given argument has type '%s'",
				fn->getArguments()[i]->getType()->str().c_str(), val->getType()->str().c_str());
		}

		args.push_back(val);
		i++;
	}

	auto res = cs->irb.CreateCall(fn, args);
	return CGResult(res);
}

















