// IRBlock.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "../include/ir/function.h"
#include "../include/ir/block.h"

namespace fir
{
	IRBlock::IRBlock() : Value(PrimitiveType::getVoid())
	{
		this->parentFunction = 0;
	}

	IRBlock::IRBlock(Function* fn) : Value(PrimitiveType::getVoid())
	{
		this->parentFunction = fn;
		this->addUser(fn);
	}

	void IRBlock::setFunction(Function* fn)
	{
		this->parentFunction = fn;
		this->addUser(fn);
	}
}
