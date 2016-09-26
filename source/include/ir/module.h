// module.h
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <limits.h>

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


		GlobalVariable* createGlobalVariable(Identifier id, Type* type, ConstantValue* initVal, bool isImmut, LinkageType linkage);
		GlobalVariable* createGlobalVariable(Identifier id, Type* type, bool isImmut, LinkageType linkage);
		GlobalVariable* declareGlobalVariable(Identifier id, Type* type, bool isImmut);
		GlobalVariable* tryGetGlobalVariable(Identifier id);
		GlobalVariable* getGlobalVariable(Identifier id);


		GlobalVariable* createGlobalString(std::string str);

		std::deque<GlobalVariable*> getGlobalVariables();
		std::deque<Function*> getAllFunctions();
		std::deque<Type*> getNamedTypes();

		// note: only looks at the name + scope, excludes the parameter list.
		std::deque<Function*> getFunctionsWithName(Identifier id);
		Function* getIntrinsicFunction(std::string id);

		Type* getNamedType(Identifier name);
		void addNamedType(Identifier name, Type* type);

		void addFunction(Function* func);

		Function* declareFunction(Identifier id, FunctionType* ftype);
		Function* getFunction(Identifier id);
		Function* getOrCreateFunction(Identifier id, FunctionType* ftype, LinkageType linkage);

		std::string getModuleName();
		void setModuleName(std::string name);

		llvm::Module* translateToLlvm();

		std::string print();

		private:
		std::string moduleName;
		std::unordered_map<std::string, GlobalVariable*> globalStrings;

		std::unordered_map<Identifier, GlobalVariable*> globals;
		std::unordered_map<Identifier, Function*> functions;
		std::unordered_map<Identifier, Type*> namedTypes;

		std::unordered_map<Identifier, Function*> intrinsicFunctions;
	};


	struct ExecutionTarget
	{
		size_t getBitsPerByte();
		size_t getPointerWidthInBits();
		size_t getTypeSizeInBits(Type* type);

		Type* getPointerSizedIntegerType();

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


































