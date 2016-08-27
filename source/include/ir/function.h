// function.h
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <limits.h>

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
		~Argument();

		Value* getActualValue();
		Function* getParentFunction();


		void setValue(Value* v);
		void clearValue();

		// fields
		protected:
		Function* parentFunction;
		Value* realValue = 0;
	};


	struct Function : GlobalValue
	{
		friend struct Module;
		friend struct Argument;
		friend struct IRBuilder;

		bool isCStyleVarArg();
		bool isVariadic();

		Type* getReturnType();
		size_t getArgumentCount();
		std::deque<Argument*> getArguments();

		std::deque<IRBlock*>& getBlockList();
		void deleteBody();

		// overridden stuff
		virtual FunctionType* getType() override; // override because better (more specific) return type.
		Function* reify(std::map<std::string, Type*> names, FTContext* tc = 0);


		static Function* create(Identifier name, FunctionType* fnType, Module* module, LinkageType linkage);


		// fields
		protected:
		Function(Identifier name, FunctionType* fnType, Module* module, LinkageType linkage);
		std::deque<Argument*> fnArguments;
		std::deque<IRBlock*> blocks;
	};
}














































