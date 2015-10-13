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

namespace flax
{
	struct Function;
	struct IRBuilder;
	struct Argument : Value
	{
		friend struct Function;
		friend struct IRBuilder;

		protected:
		// virtual stuff
		// default: virtual Type* getType() const


		// methods
		Argument(Function* fn, Type* type);
		Value* getActualValue();
		Function* getParentFunction();

		void setValue(Value* v);
		void clearValue();

		// fields
		Function* parentFunction;
		Value* realValue = 0;
	};

	struct Function : Value
	{
		friend struct Argument;

		protected:
		Function(std::string name, FunctionType* fnType);

		std::string getName();
		Type* getReturnType();
		std::deque<Argument*> getArguments();


		// overridden stuff
		virtual FunctionType* getType() const override; // override because better (more specific) return type.


		// fields
		std::deque<Argument*> fnArguments;
	};
}














































