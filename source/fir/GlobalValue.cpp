// GlobalValue.cpp
// Copyright (c) 2014 - 2016, zhiayang
// Licensed under the Apache License Version 2.0.


#include "ir/value.h"
#include "ir/module.h"
#include "ir/constant.h"

namespace fir
{
	GlobalValue::GlobalValue(Module* m, Type* type, LinkageType linkage, bool mut) : ConstantValue(type)
	{
		this->linkageType = linkage;
		this->parentModule = m;

		//* not a typo; only variables deserve to be 'lvalue', global values (eg. functions)
		//* should just be rvalues.
		if(mut) this->kind = Kind::lvalue;
		else    this->kind = Kind::prvalue;
	}


	GlobalVariable::GlobalVariable(const Identifier& name, Module* module, Type* type, bool immutable, LinkageType lt, ConstantValue* initValue)
		: GlobalValue(module, type, lt, !immutable)
	{
		this->ident = name;
		this->initValue = initValue;

		this->kind = Kind::lvalue;
		this->isconst = immutable;
	}

	void GlobalVariable::setInitialValue(ConstantValue* constVal)
	{
		if(constVal && constVal->getType() != this->getType())
			error("storing value of '%s' in global var of type '%s'", constVal->getType(), this->getType());

		this->initValue = constVal;
	}

	ConstantValue* GlobalVariable::getInitialValue()
	{
		return this->initValue;
	}
}






















