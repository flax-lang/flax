// function.h
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
#include "block.h"

namespace fir
{
	struct Function;
	struct IRBuilder;
	struct Argument : Value
	{
		friend struct Function;
		friend struct IRBuilder;

		// virtual stuff
		// default: virtual Type* getType()


		// methods
		Argument(Function* fn, Type* type);
		Value* getActualValue();
		Function* getParentFunction();


		protected:
		void setValue(Value* v);
		void clearValue();

		// fields
		Function* parentFunction;
		Value* realValue = 0;
	};


	struct Function : GlobalValue
	{
		friend struct Module;
		friend struct Argument;
		friend struct IRBuilder;

		bool isVarArg();
		std::string getName();
		Type* getReturnType();
		size_t getArgumentCount();
		std::deque<Argument*> getArguments();

		std::deque<IRBlock*>& getBlockList();
		void deleteBody();

		// overridden stuff
		virtual FunctionType* getType() override; // override because better (more specific) return type.


		// fields
		protected:
		Function(std::string name, FunctionType* fnType, Module* module, LinkageType linkage);
		std::deque<Argument*> fnArguments;
		std::deque<IRBlock*> blocks;
	};
}














































