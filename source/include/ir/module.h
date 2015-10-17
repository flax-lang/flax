// module.h
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

#include <map>
#include <deque>
#include <string>
#include <vector>
#include <unordered_map>

#include "value.h"
#include "function.h"

namespace fir
{
	struct Module
	{
		Module(std::string nm) : moduleName(nm) { }

		GlobalVariable* createGlobalVariable(std::string name, Type* type, Value* initVal);
		GlobalVariable* createGlobalVariable(std::string name, Type* type);
		void deleteGlobalVariable(std::string name);

		GlobalVariable* getGlobalVariable(std::string name);

		std::deque<GlobalValue*> getGlobalVariables();
		std::deque<StructType*> getNamedTypes();
		std::deque<Function*> getAllFunctions();

		StructType* getNamedType(std::string name);
		void addNamedType(std::string name, StructType* type);
		void deleteNamedType(std::string name);

		void declareFunction(std::string name, FunctionType* ftype);
		void addFunction(Function* func);
		void deleteFunction(std::string name);
		Function* getFunction(std::string name);

		std::string getModuleName();
		void setModuleName(std::string name);

		private:
		std::string moduleName;
		std::map<std::string, GlobalValue*> globals;
		std::map<std::string, StructType*> namedTypes;
		std::map<std::string, Function*> functions;
	};

	struct ExecutionTarget
	{
		size_t getPointerWidthInBits();
		size_t getTypeSizeInBits(Type* type);
	};
}


































