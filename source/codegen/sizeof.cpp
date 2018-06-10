// sizeof.cpp
// Copyright (c) 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "sst.h"
#include "codegen.h"

CGResult sst::SizeofOp::_codegen(cgn::CodegenState* cs, fir::Type* inferred)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	auto sz = fir::ConstantInt::getInt64(cs->module->getSizeOfType(this->typeToSize));
	return CGResult(sz);
}


CGResult sst::TypeidOp::_codegen(cgn::CodegenState* cs, fir::Type* inferred)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	auto sz = fir::ConstantInt::getUint64(this->typeToId->getID());
	return CGResult(sz);
}