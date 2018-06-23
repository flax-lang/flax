// structs.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "errors.h"
#include "codegen.h"
#include "typecheck.h"

CGResult sst::StructDefn::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	iceAssert(this->type && this->type->isStructType());
	// cs->typeDefnMap[this->type] = this;

	for(auto nt : this->nestedTypes)
		nt->codegen(cs);

	for(auto method : this->methods)
		method->codegen(cs);

	for(auto sm : this->staticFields)
		sm->codegen(cs);

	for(auto sm : this->staticMethods)
		sm->codegen(cs);

	return CGResult(0);
}



































