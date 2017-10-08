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
		warn(this, "globals not supported");
	}

	fir::Value* val = 0;
	fir::Value* alloc = 0;

	CGResult res;
	bool refcounted = cs->isRefCountedType(this->type);
	if(this->init)
	{
		res = this->init->codegen(cs, this->type);
		iceAssert(res.value);

		val = res.value;
	}

	if(!val) val = cs->getDefaultValue(this->type);


	fir::Value* nv = val;
	if(val->getType() != this->type)
		nv = cs->oneWayAutocast(CGResult(val), this->type).value;


	if(!nv)
	{
		iceAssert(this->init);

		HighlightOptions hs;
		hs.underlines.push_back(this->init->loc);
		error(this, hs, "Cannot initialise variable of type '%s' with a value of type '%s'", this->type->str(), val->getType()->str());
	}


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
	cs->vtree->values[this->id.name].push_back(CGResult(0, alloc, CGResult::VK::LValue));

	if(refcounted) cs->addRefCountedPointer(alloc);

	return CGResult(0, alloc);
}

static CGResult findValueInVTree(cgn::ValueTree* vtr, std::string name)
{
	cgn::ValueTree* tree = vtr;
	while(tree)
	{
		auto vs = tree->values[name];
		iceAssert(vs.size() <= 1);

		if(vs.size() > 0)
			return vs[0];

		tree = tree->parent;
	}

	return CGResult(0);
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
			auto r = findValueInVTree(cs->vtree, this->name);
			if(r.value || r.pointer)
			{
				defn = r;
			}
			else if(cs->isInMethodBody())
			{
				fir::Value* self = cs->getMethodSelf();
				auto ty = self->getType();

				iceAssert(ty->isPointerType() && ty->getPointerElementType()->isStructType());
				auto sty = ty->getPointerElementType()->toStructType();

				if(sty->hasElementWithName(this->name))
				{
					// ok -- return directly from here.
					fir::Value* ptr = cs->irb.CreateGetStructMember(self, this->name);
					return CGResult(cs->irb.CreateLoad(ptr), ptr, CGResult::VK::LValue);
				}
			}
			else if(!defn.pointer && !defn.value)
			{
				error(this, "wtf no");
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
		error(this, "Type mismatch; typechecking found type '%s', codegen gave type '%s'", this->type->str(), value->getType()->str());

	return CGResult(value, defn.pointer, CGResult::VK::LValue);
}
















