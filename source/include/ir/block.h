// block.h
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <limits.h>

#include "errors.h"

#include <string>
#include <vector>
#include <deque>
#include <unordered_map>

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

		private:
		Function* parentFunction = 0;
		std::deque<Instruction*> instructions;
	};
}



















