// block.h
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <limits.h>

#ifndef __STDC_CONSTANT_MACROS
#define __STDC_CONSTANT_MACROS
#endif

#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS
#endif

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
		friend struct IRBuilder;
		IRBlock();
		IRBlock(Function* parentFunc);

		void setFunction(Function* fn);
		void addInstruction(Instruction* inst);

		private:
		Function* parentFunction = 0;
		std::deque<Instruction*> instructions;
	};
}



















