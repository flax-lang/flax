// variable.cpp
// Copyright (c) 2014 - 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include "sst.h"
#include "codegen.h"


CGResult sst::VarDefn::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	auto checkStore = [this, cs](fir::Value* val) -> fir::Value* {

		fir::Value* nv = val;
		if(nv->getType() != this->type)
			nv = cs->oneWayAutocast(nv, this->type);

		if(nv->getType() != this->type)
		{
			iceAssert(this->init);

			SpanError::make(SimpleError::make(this->loc, "cannot initialise variable of type '%s' with a value of type '%s'", this->type, nv->getType()))
				->add(util::ESpan(this->init->loc, strprintf("type '%s'", nv->getType())))
				->postAndQuit();
		}

		return nv;
	};

	if(auto it = cs->typeDefnMap.find(this->type); it != cs->typeDefnMap.end())
		it->second->codegen(cs);

	if(this->global)
	{
		auto rest = cs->enterGlobalInitFunction();

		// else
		fir::Value* res = 0;

		if(this->init)  res = this->init->codegen(cs, this->type).value;
		else            res = cs->getDefaultValue(this->type);

		//* note: we declare it as not-immutable here to make it easier to set things, but otherwise we make it immutable again below after init.
		auto glob = cs->module->createGlobalVariable(this->id, this->type, false,
			this->visibility == VisibilityLevel::Public ? fir::LinkageType::External : fir::LinkageType::Internal);

		if(auto cv = dcast(fir::ConstantValue, res); cv && cv->getType() == this->type)
		{
			glob->setInitialValue(cv);
		}
		else
		{
			res = checkStore(res);
			cs->autoAssignRefCountedValue(glob, res, true);
		}

		// go and fix the thing.
		if(this->immutable)
			glob->makeConst();

		cs->leaveGlobalInitFunction(rest);

		cs->valueMap[this] = CGResult(glob);
		return CGResult(glob);
	}
	else
	{
		fir::Value* val = 0;

		if(this->init)
		{
			val = this->init->codegen(cs, this->type).value;
			val = cs->oneWayAutocast(val, this->type);
		}
		else
		{
			val = cs->getDefaultValue(this->type);
			val = checkStore(val);
		}

		auto alloc = cs->irb.CreateLValue(this->type, this->id.name);
		cs->autoAssignRefCountedValue(alloc, val, /* isInitial: */ true);

		if(this->immutable)
			alloc->makeConst();

		cs->addVariableUsingStorage(this, alloc);

		return CGResult(alloc);
	}
}

void cgn::CodegenState::addVariableUsingStorage(sst::VarDefn* var, fir::Value* alloc)
{
	iceAssert(alloc);
	this->valueMap[var] = CGResult(alloc);

	if(fir::isRefCountedType(var->type))
		this->addRefCountedValue(alloc);

	if(var->type->isClassType())
		this->addRAIIValue(alloc);
}










CGResult sst::VarRef::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	fir::Value* value = 0;
	{
		auto it = cs->valueMap.find(this->def);

		if(it != cs->valueMap.end())
		{
			value = it->second.value;
		}
		else
		{
			if(this->isImplicitField)
			{
				iceAssert(cs->isInMethodBody());
				return cs->getStructFieldImplicitly(this->name);
			}
			else
			{
				this->def->codegen(cs);

				it = cs->valueMap.find(this->def);
				if(it == cs->valueMap.end())
				{
					SimpleError::make(this->loc, "failed to codegen variable definition for '%s'", this->name)
						->append(SimpleError::make(MsgType::Note, this->def->loc, "offending definition is here:"))
						->postAndQuit();
				}

				value = it->second.value;
			}
		}
	}

	// make sure types match... should we bother?
	if(value->getType() != this->type)
		error(this, "type mismatch; typechecking found type '%s', codegen gave type '%s'", this->type, value->getType());

	return CGResult(value);
}





CGResult sst::SelfVarRef::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	iceAssert(cs->isInMethodBody());
	return CGResult(cs->getMethodSelf());
}




















