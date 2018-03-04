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

	auto checkStore = [this, cs](fir::Value* val) -> fir::Value* {

		fir::Value* nv = val;
		if(val->getType() != this->type)
			nv = cs->oneWayAutocast(CGResult(val), this->type).value;

		if(!nv)
		{
			iceAssert(this->init);

			HighlightOptions hs;
			hs.underlines.push_back(this->init->loc);
			error(this, hs, "Cannot initialise variable of type '%s' with a value of type '%s'", this->type, val->getType());
		}

		return nv;
	};

	if(auto it = cs->typeDefnMap.find(this->type); it != cs->typeDefnMap.end())
		it->second->codegen(cs);

	bool refcounted = cs->isRefCountedType(this->type);

	if(this->global)
	{
		auto rest = cs->enterGlobalInitFunction();

		// else
		CGResult res;

		if(this->init)
			res = this->init->codegen(cs, this->type);

		else
			res = CGResult(cs->getDefaultValue(this->type), 0, CGResult::VK::LitRValue);

		fir::Value* val = checkStore(res.value);

		//* note: we declare it as not-immutable here to make it easier to set things, but otherwise we make it immutable again below after init.
		fir::Value* alloc = cs->module->createGlobalVariable(this->id, this->type, false, this->visibility == VisibilityLevel::Public ? fir::LinkageType::External : fir::LinkageType::Internal);

		cs->autoAssignRefCountedValue(CGResult(val, alloc), res, true, true);

		//* here:
		if(this->immutable)
			alloc->makeImmutable();

		cs->leaveGlobalInitFunction(rest);

		cs->valueMap[this] = CGResult(0, alloc, CGResult::VK::LValue);

		return CGResult(0, alloc);
	}






	fir::Value* val = 0;
	fir::Value* alloc = 0;

	CGResult res;
	if(this->init)
	{
		res = this->init->codegen(cs, this->type);
		iceAssert(res.value);

		val = res.value;
	}

	if(!val) val = cs->getDefaultValue(this->type);


	val = checkStore(val);

	if(this->immutable)
	{
		iceAssert(val);
		alloc = cs->irb.ImmutStackAlloc(this->type, val, this->id.name);
	}
	else
	{
		alloc = cs->irb.StackAlloc(this->type, this->id.name);
	}

	iceAssert(alloc);

	cs->addVariableUsingStorage(this, alloc, res);

	return CGResult(0, alloc);
}

void cgn::CodegenState::addVariableUsingStorage(sst::VarDefn* var, fir::Value* alloc, CGResult val)
{
	iceAssert(alloc);
	this->valueMap[var] = CGResult(0, alloc, CGResult::VK::LValue);

	if(val.value || val.pointer)
		this->autoAssignRefCountedValue(CGResult(0, alloc), val, true, !var->immutable);

	if(this->isRefCountedType(var->type))
		this->addRefCountedPointer(alloc);
}










CGResult sst::VarRef::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	CGResult defn;
	{
		auto it = cs->valueMap.find(this->def);

		if(it != cs->valueMap.end())
		{
			defn = it->second;
		}
		else
		{
			if(cs->isInMethodBody())
			{
				return cs->getStructFieldImplicitly(this->name);
			}
			else if(!defn.pointer && !defn.value)
			{
				// warn(this, "forcing codegen of this");
				// warn(this->def, "here");
				this->def->codegen(cs);

				it = cs->valueMap.find(this->def);
				if(it == cs->valueMap.end())
				{
					exitless_error(this, "Failed to codegen variable definition for '%s'", this->name);
					info(this->def, "Offending definition is here:");
					doTheExit();
				}

				defn = it->second;
			}
		}
	}

	fir::Value* value = 0;
	if(!defn.pointer)
	{
		iceAssert(defn.value);
		value = defn.value;
	}
	else
	{
		iceAssert(defn.pointer);
		value = cs->irb.Load(defn.pointer);
	}

	// make sure types match... should we bother?
	if(value->getType() != this->type)
		error(this, "Type mismatch; typechecking found type '%s', codegen gave type '%s'", this->type, value->getType());

	return CGResult(value, defn.pointer, CGResult::VK::LValue);
}
















