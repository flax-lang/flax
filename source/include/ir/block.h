// block.h
// Copyright (c) 2014 - 2016, zhiayang
// Licensed under the Apache License Version 2.0.

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <limits.h>

#include "value.h"
#include "instruction.h"

namespace fir
{
	struct Function;
	struct IRBuilder;

	struct IRBlock : Value
	{
		friend struct Module;
		friend struct IRBuilder;

		IRBlock();
		IRBlock(Function* parentFunc);

		Function* getParentFunction();

		void setFunction(Function* fn);
		void addInstruction(Instruction* inst);
		void eraseFromParentFunction();

		bool isTerminated();

		std::vector<Instruction*>& getInstructions();

		private:
		Function* parentFunction = 0;
		std::vector<Instruction*> instructions;
	};
}



















