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


	bool refcounted = cs->isRefCountedType(this->type);

	if(this->global)
	{
		auto rest = cs->enterGlobalInitFunction();

		// else
		CGResult res;

		if(this->init)
			res = this->init->codegen(cs, this->type);

		else
			res = CGResult(cs->getDefaultValue(this->type));

		fir::Value* val = checkStore(res.value);
		fir::Value* alloc = cs->module->createGlobalVariable(this->id, this->type, this->immutable, this->visibility == VisibilityLevel::Public ? fir::LinkageType::External : fir::LinkageType::Internal);

		if(refcounted)
		{
			if(res.kind == CGResult::VK::LValue)
				cs->performRefCountingAssignment(CGResult(val, alloc), res, true);

			else
				cs->moveRefCountedValue(CGResult(val, alloc), res, true);
		}

		if(!refcounted)
		{
			alloc->makeNotImmutable();
			cs->irb.CreateStore(val, alloc);

			if(this->immutable)
				alloc->makeImmutable();
		}

		cs->leaveGlobalInitFunction(rest);

		cs->valueMap[this] = CGResult(0, alloc, CGResult::VK::LValue);
		// cs->vtree->values[this->id.name].push_back(CGResult(0, alloc, CGResult::VK::LValue));

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
		alloc = cs->irb.CreateImmutStackAlloc(this->type, val, this->id.name);
	}
	else
	{
		alloc = cs->irb.CreateStackAlloc(this->type, this->id.name);
	}

	iceAssert(alloc);
	if(this->init)
	{
		if(refcounted)
		{
			if(res.kind == CGResult::VK::LValue)
				cs->performRefCountingAssignment(CGResult(val, alloc), res, true);

			else
				cs->moveRefCountedValue(CGResult(val, alloc), res, true);
		}

		if(!this->immutable)
			cs->irb.CreateStore(val, alloc);
	}

	cs->valueMap[this] = CGResult(0, alloc, CGResult::VK::LValue);
	// cs->vtree->values[this->id.name].push_back(CGResult(0, alloc, CGResult::VK::LValue));

	if(refcounted) cs->addRefCountedPointer(alloc);

	return CGResult(0, alloc);
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
					// auto r = cs->findValueInTree(this->name);
					// if(r.value || r.pointer)
					// {
					// 	defn = r;
					// }
					// else
					// {
						exitless_error(this, "Failed to codegen variable definition for '%s'", this->name);
						info(this->def, "Offending definition is here:");
						doTheExit();
					// }
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
		value = cs->irb.CreateLoad(defn.pointer);
	}

	// make sure types match... should we bother?
	if(value->getType() != this->type)
		error(this, "Type mismatch; typechecking found type '%s', codegen gave type '%s'", this->type, value->getType());

	return CGResult(value, defn.pointer, CGResult::VK::LValue);
}
















