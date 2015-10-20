// GlobalValue.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "ir/value.h"
#include "ir/module.h"
#include "ir/constant.h"

namespace fir
{
	GlobalValue::GlobalValue(Type* type, LinkageType linkage) : Value(type)
	{
		this->linkageType = linkage;
	}


	GlobalVariable::GlobalVariable(std::string name, Module* module, Type* type, bool immutable, LinkageType lt, ConstantValue* initValue)
		: GlobalValue(type, lt)
	{
		this->valueName = name;
		this->parentModule = module;
		this->isImmutable = immutable;
		this->initValue = initValue;
	}

	void GlobalVariable::setInitialValue(ConstantValue* constVal)
	{
		this->initValue = constVal;
	}
}






















