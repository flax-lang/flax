// Value.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "../include/ir/value.h"
#include "../include/ir/constant.h"

namespace fir
{
	Value::Value(Type* t)
	{
		static size_t vnames = 0;
		this->valueType = t;

		this->id = vnames;
		this->valueName = "v#" + std::to_string(vnames);

		vnames++;
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

	void Value::addUser(Value* user)
	{
		for(auto v : this->users)
			if(v == user) return;

		this->users.push_back(user);
	}

	void Value::transferUsesTo(Value* other)
	{
		// check.
		std::deque<Value*> culled;

		// todo: O(N^2)
		for(auto v : this->users)
		{
			bool found = false;
			for(auto ov : other->users)
			{
				if(v == ov)
				{
					found = true;
					break;
				}
			}

			if(!found)
				culled.push_back(v);
		}

		for(auto c : culled)
			other->users.push_back(c);
	}
}






















