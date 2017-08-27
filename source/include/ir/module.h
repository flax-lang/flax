// module.h
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <limits.h>

#include "value.h"
#include "function.h"

namespace llvm
{
	class Module;
}


namespace fir
{
	struct ExecutionTarget;

	struct Module
	{
		Module(std::string nm);

		GlobalVariable* createGlobalVariable(const Identifier& id, Type* type, ConstantValue* initVal, bool isImmut, LinkageType linkage);
		GlobalVariable* createGlobalVariable(const Identifier& id, Type* type, bool isImmut, LinkageType linkage);
		GlobalVariable* declareGlobalVariable(const Identifier& id, Type* type, bool isImmut);
		GlobalVariable* tryGetGlobalVariable(const Identifier& id);
		GlobalVariable* getGlobalVariable(const Identifier& id);


		GlobalVariable* createGlobalString(std::string str);

		std::vector<GlobalVariable*> getGlobalVariables();
		std::vector<Function*> getAllFunctions();
		std::vector<Type*> getNamedTypes();

		// note: only looks at the name + scope, excludes the parameter list.
		std::vector<Function*> getFunctionsWithName(const Identifier& id);
		Function* getIntrinsicFunction(std::string id);

		Type* getNamedType(const Identifier& name);
		void addNamedType(const Identifier& name, Type* type);

		void addFunction(Function* func);
		void removeFunction(Function* func);

		Function* declareFunction(const Identifier& id, FunctionType* ftype);
		Function* getFunction(const Identifier& id);
		Function* getOrCreateFunction(const Identifier& id, FunctionType* ftype, LinkageType linkage);

		std::string getModuleName();
		void setModuleName(std::string name);

		std::string print();

		void setExecutionTarget(ExecutionTarget* e);
		ExecutionTarget* getExecutionTarget();

		Function* getEntryFunction();
		void setEntryFunction(Function* fn);


		std::unordered_map<Identifier, Function*> _getIntrinsicFunctions() { return this->intrinsicFunctions; }
		std::unordered_map<std::string, GlobalVariable*> _getGlobalStrings() { return this->globalStrings; }
		std::unordered_map<Identifier, GlobalVariable*> _getGlobals() { return this->globals; }
		std::unordered_map<Identifier, Function*> _getFunctions() { return this->functions; }
		std::unordered_map<Identifier, Type*> _getNamedTypes() { return this->namedTypes; }



		private:
		std::string moduleName;
		std::unordered_map<std::string, GlobalVariable*> globalStrings;

		std::unordered_map<Identifier, GlobalVariable*> globals;
		std::unordered_map<Identifier, Function*> functions;
		std::unordered_map<Identifier, Type*> namedTypes;

		std::unordered_map<Identifier, Function*> intrinsicFunctions;

		Function* entryFunction = 0;

		ExecutionTarget* execTarget = 0;
	};


	struct ExecutionTarget
	{
		size_t getBitsPerByte();
		size_t getPointerWidthInBits();
		size_t getTypeSizeInBits(Type* type);
		size_t getTypeSizeInBytes(Type* type);

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


































