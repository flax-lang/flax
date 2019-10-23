// Value.cpp
// Copyright (c) 2014 - 2016, zhiayang
// Licensed under the Apache License Version 2.0.

#include "ir/value.h"
#include "ir/constant.h"

namespace fir
{
	static size_t vnames = 0;
	Value::Value(Type* t, Kind k) : ident(), valueType(t), kind(k)
	{
		this->id = vnames++;
	}

	Type* Value::getType()
	{
		if(this->valueType) return this->valueType;

		error("value has no type????");
	}

	bool Value::hasName()
	{
		return this->ident.str() != "";
	}

	void Value::setName(const Identifier& name)
	{
		this->ident = name;
	}

	void Value::setName(const std::string& name)
	{
		this->ident = Identifier(name, IdKind::Name);
	}

	const Identifier& Value::getName()
	{
		return this->ident;
	}

	size_t Value::getCurrentValueId()
	{
		return vnames;
	}







	void PHINode::addIncoming(Value* v, IRBlock* block)
	{
		iceAssert(v->getType() == this->valueType && "types not identical");
		if(this->incoming.find(block) != this->incoming.end())
			iceAssert(0 && "block already has incoming value");

		this->incoming[block] = v;
	}

	std::map<IRBlock*, Value*> PHINode::getValues()
	{
		return this->incoming;
	}

	PHINode::PHINode(Type* t) : fir::Value(t)
	{
	}
}






















