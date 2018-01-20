// sizeof.cpp
// Copyright (c) 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "sst.h"
#include "codegen.h"

CGResult sst::SizeofOp::_codegen(cgn::CodegenState* cs, fir::Type* inferred)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	auto type = this->typeToSize;
	iceAssert(type);

	// let llvm handle everything :D
	// ie. use GEP.
	auto nullp = fir::ConstantValue::getZeroValue(type->getPointerTo());
	auto sz = cs->irb.PointerToIntCast(cs->irb.GetPointer(nullp, fir::ConstantInt::getInt64(1)), fir::Type::getInt64());

	return CGResult(sz);
}