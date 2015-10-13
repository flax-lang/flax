// Value.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "../include/ir/value.h"

namespace flax
{
	Value::Value(Type* t)
	{
		static size_t vnames = 0;
		this->valueType = t;

		this->valueName = "v#" + std::to_string(vnames++);
	}

	Type* Value::getType()
	{
		if(this->valueType) return this->valueType;

		iceAssert(0 && "Value has no type????");
	}

	void Value::setName(std::string name)
	{
		this->valueName = name;
	}

	std::string Value::getName()
	{
		return this->valueName;
	}





	ConstantValue::ConstantValue(Type* t) : Value(t)
	{
		// nothing.
	}

	ConstantValue* ConstantValue::getNullValue(Type* type)
	{
		return new ConstantValue(type);
	}










	// todo: unique these values.
	ConstantInt* ConstantInt::getConstantSIntValue(Type* intType, ssize_t val)
	{
		iceAssert(intType->isIntegerType() && "not integer type");
		ConstantInt* ret = new ConstantInt(intType, val);

		return ret;
	}

	ConstantInt* ConstantInt::getConstantUIntValue(Type* intType, size_t val)
	{
		iceAssert(intType->isIntegerType() && "not integer type");
		ConstantInt* ret = new ConstantInt(intType, val);

		return ret;
	}

	ConstantInt::ConstantInt(Type* type, ssize_t val) : flax::ConstantValue(type)
	{
		this->value = val;
	}

	ConstantInt::ConstantInt(Type* type, size_t val) : flax::ConstantValue(type)
	{
		this->value = val;
	}
}






















