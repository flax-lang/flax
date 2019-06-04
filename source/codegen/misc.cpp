// misc.cpp
// Copyright (c) 2014 - 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include "sst.h"
#include "codegen.h"

CGResult sst::TypeExpr::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	return CGResult(fir::ConstantValue::getZeroValue(this->type));
}

CGResult sst::ScopeExpr::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	error(this, "Failed to resolve scope '%s'", util::serialiseScope(this->scope));
}

CGResult sst::TreeDefn::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	error(this, "Cannot codegen tree definition -- something fucked up somewhere");
}

CGResult sst::BareTypeDefn::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	// there's nothing to do here...

	cs->pushLoc(this);
	defer(cs->popLoc());

	return CGResult(0);
}










