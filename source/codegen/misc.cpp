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
	error(this, "failed to resolve scope '%s'", this->scope.string());
}

CGResult sst::BareTypeDefn::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	// there's nothing to do here...

	cs->pushLoc(this);
	defer(cs->popLoc());

	return CGResult(0);
}

CGResult sst::Stmt::codegen(cgn::CodegenState* cs, fir::Type* inferred)
{
	return this->_codegen(cs, inferred);
}

CGResult sst::Defn::codegen(cgn::CodegenState* cs, fir::Type* inferred)
{
	if(this->didCodegen && cs->id == this->cachedCSId)
		return this->cachedResult;

	this->didCodegen = true;
	this->cachedCSId = cs->id;
	return (this->cachedResult = this->_codegen(cs, inferred));
}

// TODO: move this impl somewhere else?
sst::FunctionDefn* cgn::CodegenState::findMatchingMethodInType(sst::TypeDefn* td, sst::FunctionDecl* fn)
{
	if(auto str = dcast(sst::StructDefn, td); str)
	{
		// TODO: when (if) we figure out what's going on in typecheck/traits.cpp:129, possibly change this to match.
		auto it = std::find_if(str->methods.begin(), str->methods.end(), [fn](sst::FunctionDefn* method) -> bool {

			//* i think this check should work, `areMethodsVirtuallyCompatible` basically checks the parameters but takes
			//* co/contravariance into account and doesn't match the first (self) parameter.
			return (fn->id.name == method->id.name && fir::areMethodsVirtuallyCompatible(
				fn->type->toFunctionType(), method->type->toFunctionType(), /* checking trait: */ true)
			);
		});

		if(it != str->methods.end())
			return *it;
	}

	return 0;
}

































