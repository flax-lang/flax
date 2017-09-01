// variable.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "sst.h"
#include "codegen.h"


CGResult sst::VarDefn::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	// ok.
	// add a new thing to the thing

	if(this->global)
	{
		warn(this, "no");
	}

	fir::Value* val = 0;
	fir::Value* alloc = 0;

	if(this->init)
	{
		auto res = this->init->codegen(cs, this->type);
		iceAssert(res.value);

		val = res.value;
	}

	if(this->immutable)
	{
		iceAssert(val);
		alloc = cs->irb.CreateImmutStackAlloc(this->type, val);
	}
	else
	{
		alloc = cs->irb.CreateStackAlloc(this->type);
		if(!val)
			val = cs->getDefaultValue(this->type);

		cs->irb.CreateStore(val, alloc);
	}

	cs->valueMap[this] = CGResult(0, alloc, CGResult::VK::LValue);
	cs->vtree->values[this->id.name].push_back(CGResult(0, alloc, CGResult::VK::LValue));
	return CGResult(alloc);
}

CGResult sst::VarRef::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	auto it = cs->valueMap.find(this->def);
	if(it == cs->valueMap.end())
		error("wtf?");

	fir::Value* value = 0;
	auto defn = it->second;
	if(!defn.value)
	{
		iceAssert(defn.pointer);
		value = cs->irb.CreateLoad(defn.pointer);
	}
	else
	{
		iceAssert(defn.value);
		value = defn.value;
	}

	// make sure types match... should we bother?
	if(value->getType() != this->type)
		error(this, "Type mismatch; typechecking found type '%s', codegen gave type '%s'", this->type->str(), value->getType()->str());

	return CGResult(value);
}
















