// variable.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "sst.h"
#include "codegen.h"

CGResult sst::VarDefn::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	return CGResult(0);
}

CGResult sst::VarRef::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	return CGResult(0);
}
