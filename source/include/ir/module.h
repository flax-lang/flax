// module.h
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <limits.h>

#include "errors.h"

#include <map>
#include <deque>
#include <string>
#include <vector>
#include <unordered_map>

#include "value.h"
#include "function.h"

namespace llvm
{
	class Module;
}

namespace fir
{
	struct Module
	{
		Module(std::string nm);

		GlobalVariable* createGlobalVariable(std::string name, Type* type, ConstantValue* initVal, bool isImmut, LinkageType linkage);
		GlobalVariable* createGlobalVariable(std::string name, Type* type, bool isImmut, LinkageType linkage);
		GlobalVariable* declareGlobalVariable(std::string name, Type* type, bool isImmut);

		GlobalVariable* createGlobalString(std::string str);

		void deleteGlobalVariable(std::string name);

		GlobalVariable* getGlobalVariable(std::string name);

		std::deque<GlobalVariable*> getGlobalVariables();
		std::deque<StructType*> getNamedTypes();
		std::deque<Function*> getAllFunctions();

		StructType* getNamedType(std::string name);
		void addNamedType(std::string name, StructType* type);
		void deleteNamedType(std::string name);

		void declareFunction(std::string name, FunctionType* ftype);
		void addFunction(Function* func);
		void deleteFunction(std::string name);
		Function* getFunction(std::string name);

		Function* getOrCreateFunction(std::string name, FunctionType* ftype, LinkageType linkage);

		std::string getModuleName();
		void setModuleName(std::string name);

		llvm::Module* translateToLlvm();

		std::string print();

		private:
		std::string moduleName;
		std::map<std::string, GlobalVariable*> globalStrings;
		std::map<std::string, GlobalVariable*> globals;
		std::map<std::string, StructType*> namedTypes;
		std::map<std::string, Function*> functions;
	};


	struct ExecutionTarget
	{
		size_t getBitsPerByte();
		size_t getPointerWidthInBits();
		size_t getTypeSizeInBits(Type* type);

		static ExecutionTarget* getLP64();
		static ExecutionTarget* getILP32();

		private:
		ExecutionTarget(size_t ptrSize, size_t byteSize, size_t shortSize, size_t intSize, size_t longSize);

		size_t psize;
		size_t bsize;
		size_t ssize;
		size_t isize;
		size_t lsize;
	};
}


































