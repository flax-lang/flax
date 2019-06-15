// IRBlock.cpp
// Copyright (c) 2014 - 2016, zhiayang
// Licensed under the Apache License Version 2.0.

#include "ir/function.h"
#include "ir/block.h"

namespace fir
{
	IRBlock::IRBlock() : Value(Type::getVoid())
	{
		this->parentFunction = 0;
	}

	IRBlock::IRBlock(Function* fn) : Value(Type::getVoid())
	{
		this->parentFunction = fn;
	}

	void IRBlock::setFunction(Function* fn)
	{
		this->parentFunction = fn;
	}

	Function* IRBlock::getParentFunction()
	{
		return this->parentFunction;
	}

	void IRBlock::eraseFromParentFunction()
	{
		iceAssert(this->parentFunction && "no function");
		std::vector<IRBlock*>& blist = this->parentFunction->getBlockList();

		for(auto it = blist.begin(); it != blist.end(); it++)
		{
			if(*it == this)
			{
				blist.erase(it);
				return;
			}
		}

		iceAssert(0 && "not in function");
	}

	std::vector<Instruction*>& IRBlock::getInstructions()
	{
		return this->instructions;
	}

	bool IRBlock::isTerminated()
	{
		if(this->instructions.size() == 0)
			return false;

		auto last = this->instructions.back();
		return last->opKind == OpKind::Branch_Cond
				|| last->opKind == OpKind::Branch_UnCond
				|| last->opKind == OpKind::Value_Return;
	}
}













