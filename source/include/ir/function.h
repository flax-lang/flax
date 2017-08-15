// function.h
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <limits.h>

#include "block.h"
#include "constant.h"

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
		std::vector<Argument*> getArguments();

		std::vector<IRBlock*>& getBlockList();
		void deleteBody();

		bool wasDeclaredWithBodyElsewhere();
		void setHadBodyElsewhere();

		bool isAlwaysInlined();
		void setAlwaysInline();

		void cullUnusedValues();

		// overridden stuff
		virtual FunctionType* getType() override; // override because better (more specific) return type.

		static Function* create(const Identifier& name, FunctionType* fnType, Module* module, LinkageType linkage);


		// fields
		protected:
		Function(const Identifier& name, FunctionType* fnType, Module* module, LinkageType linkage);
		std::vector<Argument*> fnArguments;
		std::vector<IRBlock*> blocks;

		bool alwaysInlined = false;
		bool hadBodyElsewhere = false;
	};
}














































