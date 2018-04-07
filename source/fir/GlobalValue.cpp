// GlobalValue.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "ir/value.h"
#include "ir/module.h"
#include "ir/constant.h"

namespace fir
{
	GlobalValue::GlobalValue(Module* m, Type* type, LinkageType linkage, bool mut) : ConstantValue(mut ? type->getMutablePointerTo() : type->getPointerTo())
	{
		this->linkageType = linkage;
		this->parentModule = m;
	}


	GlobalVariable::GlobalVariable(const Identifier& name, Module* module, Type* type, bool immutable, LinkageType lt, ConstantValue* initValue)
		: GlobalValue(module, type, lt, !immutable)
	{
		this->ident = name;
		this->initValue = initValue;
	}

	void GlobalVariable::setInitialValue(ConstantValue* constVal)
	{
		if(constVal && constVal->getType() != this->getType()->getPointerElementType())
			error("storing value of '%s' in global var of type '%s'", constVal->getType(), this->getType());

		iceAssert((!constVal || constVal->getType() == this->getType()->getPointerElementType()) && "invalid type");
		this->initValue = constVal;
	}

	ConstantValue* GlobalVariable::getInitialValue()
	{
		return this->initValue;
	}
}






















