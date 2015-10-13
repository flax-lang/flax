// Value.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "../include/flax/value.h"

namespace flax
{
	Value::Value(Type* t)
	{
		static size_t vnames = 0;
		this->valueType = t;

		this->valueName = "v#" + std::to_string(vnames++);
	}

	Type* Value::getType() const
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
}
