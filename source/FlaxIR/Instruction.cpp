// Instruction.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "../include/ir/instruction.h"

namespace fir
{

	Instruction::Instruction(OpKind kind, Type* out, std::deque<Value*> vals) : Value(out)
	{
		this->opKind = kind;
		this->operands = vals;

		this->realOutput = 0;

		for(auto v : vals)
			v->addUser(this);
	}

	Value* Instruction::getResult()
	{
		if(this->realOutput) return this->realOutput;
		iceAssert(0 && "Calling getActualValue() when not in function! (no real value)");
	}

	void Instruction::setValue(Value* v)
	{
		this->realOutput = v;
	}

	void Instruction::clearValue()
	{
		this->realOutput = 0;
	}
}
