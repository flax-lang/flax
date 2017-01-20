// Value.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ir/value.h"
#include "ir/constant.h"

namespace fir
{
	Value::Value(Type* t)
		: ident("", IdKind::Name)
	{
		static size_t vnames = 0;
		this->valueType = t;

		this->id = vnames;
		this->source = 0;
		vnames++;

		if(this->id == 8555)
		{
			// abort();
		}
	}

	Type* Value::getType()
	{
		if(this->valueType) return this->valueType;

		iceAssert(0 && "Value has no type????");
	}

	bool Value::hasName()
	{
		return this->ident.str() != "";
	}

	void Value::setName(const Identifier& name)
	{
		this->ident = name;
	}

	void Value::setName(std::string name)
	{
		this->ident = Identifier(name, IdKind::Name);
	}

	const Identifier& Value::getName()
	{
		return this->ident;
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
		std::vector<Value*> culled;

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







	void PHINode::addIncoming(Value* v, IRBlock* block)
	{
		iceAssert(v->getType() == this->valueType && "types not identical");
		if(this->incoming.find(block) != this->incoming.end())
			iceAssert(0 && "block already has incoming value");

		this->incoming[block] = v;
	}

	PHINode::PHINode(Type* t) : fir::Value(t)
	{
	}
}






















