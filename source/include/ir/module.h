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
	struct Module
	{
		friend struct GlobalValue;
		friend struct GlobalVariable;

		Module(std::string nm);

		GlobalVariable* createGlobalVariable(const Identifier& id, Type* type, ConstantValue* initVal, bool isImmut, LinkageType linkage);
		GlobalVariable* createGlobalVariable(const Identifier& id, Type* type, bool isImmut, LinkageType linkage);
		GlobalVariable* declareGlobalVariable(const Identifier& id, Type* type, bool isImmut);
		GlobalVariable* tryGetGlobalVariable(const Identifier& id);
		GlobalVariable* getGlobalVariable(const Identifier& id);

		GlobalVariable* getOrCreateVirtualTableForClass(ClassType* cls);

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

		Function* getEntryFunction();
		void setEntryFunction(Function* fn);

		const std::unordered_map<ClassType*, std::pair<std::vector<Function*>, GlobalVariable*>>& _getVtables() { return this->vtables; }
		const std::unordered_map<Identifier, Function*>& _getIntrinsicFunctions() { return this->intrinsicFunctions; }
		const std::unordered_map<std::string, GlobalVariable*>& _getGlobalStrings() { return this->globalStrings; }
		const std::unordered_map<Identifier, GlobalVariable*>& _getGlobals() { return this->globals; }
		const std::unordered_map<Identifier, Function*>& _getFunctions() { return this->functions; }
		const std::unordered_map<Identifier, Type*>& _getNamedTypes() { return this->namedTypes; }

		const std::unordered_map<size_t, GlobalValue*>& _getAllGlobals() { return this->allGlobalValues; }


		private:
		std::string moduleName;
		std::unordered_map<ClassType*, std::pair<std::vector<Function*>, GlobalVariable*>> vtables;
		std::unordered_map<std::string, GlobalVariable*> globalStrings;

		std::unordered_map<Identifier, GlobalVariable*> globals;
		std::unordered_map<Identifier, Function*> functions;
		std::unordered_map<Identifier, Type*> namedTypes;

		std::unordered_map<size_t, GlobalValue*> allGlobalValues;

		std::unordered_map<Identifier, Function*> intrinsicFunctions;

		Function* entryFunction = 0;
	};
}


































