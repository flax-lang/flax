// misc.cpp
// Copyright (c) 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include "codegen.h"
#include "platform.h"
#include "gluecode.h"
#include "frontend.h"

namespace cgn {
namespace glue {

void printRuntimeError(cgn::CodegenState* cs, fir::Value* pos, const std::string& message, const std::vector<fir::Value*>& args)
{
	//! on windows, apparently fprintf doesn't like to work.
	//! so we just use normal printf.

	if(!frontend::getIsNoRuntimeErrorStrings())
	{
		iceAssert(pos->getType()->isCharSliceType());

		fir::Value* fmtstr = cs->module->createGlobalString(("\nRuntime error at %s:\n" + message + "\n").c_str());
		fir::Value* posstr = cs->irb.GetArraySliceData(pos);

		std::vector<fir::Value*> as = { fmtstr, posstr };
		as.insert(as.end(), args.begin(), args.end());

		cs->irb.Call(cs->getOrDeclareLibCFunction("printf"), as);
	}

	cs->irb.Call(cs->getOrDeclareLibCFunction("abort"));
	cs->irb.Unreachable();

}

namespace misc
{
	using Idt = fir::Name;
	Idt getOI(const std::string& name, fir::Type* t = 0)
	{
		if(t) return fir::Name::obfuscate(name, t->encodedStr());
		else  return fir::Name::obfuscate(name);
	}

	Idt getCompare_FName(fir::Type* t)              { return getOI("compare", t); }
}
}
}



















