// module.h
// Copyright (c) 2014 - 2016, zhiayang
// Licensed under the Apache License Version 2.0.

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <limits.h>

#include "value.h"
#include "function.h"


namespace fir
{
	struct Module
	{
		friend struct GlobalValue;
		friend struct GlobalVariable;

		Module(const std::string& nm);

		GlobalVariable* createGlobalVariable(const Identifier& id, Type* type, ConstantValue* initVal, bool isImmut, LinkageType linkage);
		GlobalVariable* createGlobalVariable(const Identifier& id, Type* type, bool isImmut, LinkageType linkage);
		GlobalVariable* declareGlobalVariable(const Identifier& id, Type* type, bool isImmut);
		GlobalVariable* tryGetGlobalVariable(const Identifier& id);
		GlobalVariable* getGlobalVariable(const Identifier& id);

		GlobalVariable* getOrCreateVirtualTableForClass(ClassType* cls);

		GlobalVariable* createGlobalString(const std::string& str);

		std::vector<GlobalVariable*> getGlobalVariables();
		std::vector<Function*> getAllFunctions();
		std::vector<Type*> getNamedTypes();

		// note: only looks at the name + scope, excludes the parameter list.
		std::vector<Function*> getFunctionsWithName(const Identifier& id);
		Function* getIntrinsicFunction(const std::string& id);

		Type* getNamedType(const Identifier& name);
		void addNamedType(const Identifier& name, Type* type);

		void addFunction(Function* func);
		void removeFunction(Function* func);

		Function* declareFunction(const Identifier& id, FunctionType* ftype);
		Function* getFunction(const Identifier& id);
		Function* getOrCreateFunction(const Identifier& id, FunctionType* ftype, LinkageType linkage);

		std::string getModuleName();
		void setModuleName(const std::string& name);

		std::string print();

		Function* getEntryFunction();
		void setEntryFunction(Function* fn);


		void finaliseGlobalConstructors();

		const util::hash_map<ClassType*, std::pair<std::vector<Function*>, GlobalVariable*>>& _getVtables() { return this->vtables; }
		const util::hash_map<Identifier, Function*>& _getIntrinsicFunctions() { return this->intrinsicFunctions; }
		const util::hash_map<std::string, GlobalVariable*>& _getGlobalStrings() { return this->globalStrings; }
		const util::hash_map<Identifier, GlobalVariable*>& _getGlobals() { return this->globals; }
		const util::hash_map<Identifier, Function*>& _getFunctions() { return this->functions; }
		const util::hash_map<Identifier, Type*>& _getNamedTypes() { return this->namedTypes; }


		private:
		std::string moduleName;
		util::hash_map<ClassType*, std::pair<std::vector<Function*>, GlobalVariable*>> vtables;
		util::hash_map<std::string, GlobalVariable*> globalStrings;

		util::hash_map<Identifier, GlobalVariable*> globals;
		util::hash_map<Identifier, Function*> functions;
		util::hash_map<Identifier, Type*> namedTypes;

		util::hash_map<Identifier, Function*> intrinsicFunctions;

		Function* entryFunction = 0;
	};
}


































