// Module.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "../include/ir/module.h"

namespace fir
{
	Module::Module(std::string nm)
	{
		this->moduleName = nm;
	}

	GlobalVariable* Module::createGlobalVariable(std::string name, Type* type, ConstantValue* initVal, bool isImmut, LinkageType linkage)
	{
		GlobalVariable* gv = new GlobalVariable(name, this, type, isImmut, linkage, initVal);
		if(this->globals.find(name) != this->globals.end())
			error("ICE: Already have a global with name %s", name.c_str());

		this->globals[name] = gv;
		return gv;
	}

	GlobalVariable* Module::createGlobalVariable(std::string name, Type* type, bool isImmut, LinkageType linkage)
	{
		return this->createGlobalVariable(name, type, 0, isImmut, linkage);
	}

	GlobalVariable* Module::declareGlobalVariable(std::string name, Type* type, bool isImmut)
	{
		return this->createGlobalVariable(name, type, 0, isImmut, LinkageType::External);
	}

	void Module::deleteGlobalVariable(std::string name)
	{
		if(this->globals.find(name) == this->globals.end())
			error("ICE: no such global with name %s", name.c_str());

		// GlobalValue* gv = this->globals[name];
		this->globals.erase(name);

		// delete gv;
	}

	GlobalVariable* Module::getGlobalVariable(std::string name)
	{
		if(this->globals.find(name) == this->globals.end())
			error("ICE: no such global with name %s", name.c_str());

		return this->globals[name];
	}




	StructType* Module::getNamedType(std::string name)
	{
		if(this->namedTypes.find(name) == this->namedTypes.end())
			error("ICE: no such type with name %s", name.c_str());

		return this->namedTypes[name];
	}

	void Module::addNamedType(std::string name, StructType* type)
	{
		if(this->namedTypes.find(name) != this->namedTypes.end())
			error("ICE: type %s exists already", name.c_str());

		this->namedTypes[name] = type;
	}

	void Module::deleteNamedType(std::string name)
	{
		if(this->namedTypes.find(name) == this->namedTypes.end())
			error("ICE: no such type with name %s", name.c_str());

		this->namedTypes.erase(name);
	}

	void Module::declareFunction(std::string name, FunctionType* ftype)
	{
		this->getOrCreateFunction(name, ftype, LinkageType::External);
	}

	Function* Module::getOrCreateFunction(std::string name, FunctionType* ftype, LinkageType linkage)
	{
		if(this->functions.find(name) != this->functions.end())
		{
			if(!this->functions[name]->getType()->isTypeEqual(ftype))
			{
				error("function %s redeclared with different type (have %s, new %s)", name.c_str(),
					this->functions[name]->getType()->str().c_str(), ftype->str().c_str());
			}
		}

		Function* f = new Function(name, ftype, this, linkage);
		this->functions[name] = f;

		return f;
	}












	void Module::addFunction(Function* func)
	{
		if(this->functions.find(func->getName()) != this->functions.end())
			error("function %s exists already", func->getName().c_str());

		this->functions[func->getName()] = func;
	}

	void Module::deleteFunction(std::string name)
	{
		if(this->functions.find(name) == this->functions.end())
			error("function %s does not exist", name.c_str());

		// Function* f = this->functions[name];
		this->functions.erase(name);
	}

	Function* Module::getFunction(std::string name)
	{
		if(this->functions.find(name) == this->functions.end())
		{
			return 0;
			// error("function %s does not exist", name.c_str());
		}

		return this->functions[name];
	}

	GlobalVariable* Module::createGlobalString(std::string str)
	{
		if(this->globalStrings.find(str) != this->globalStrings.end())
			return this->globalStrings[str];

		GlobalVariable* gs = new GlobalVariable(str, this, PointerType::getInt8Ptr(), true, LinkageType::Internal, 0);
		this->globalStrings[str] = gs;

		return gs;
	}














	std::deque<GlobalVariable*> Module::getGlobalVariables()
	{
		std::deque<GlobalVariable*> ret;
		for(auto g : this->globals)
			ret.push_back(g.second);

		return ret;
	}

	std::deque<StructType*> Module::getNamedTypes()
	{
		std::deque<StructType*> ret;
		for(auto g : this->namedTypes)
			ret.push_back(g.second);

		return ret;
	}

	std::deque<Function*> Module::getAllFunctions()
	{
		std::deque<Function*> ret;
		for(auto g : this->functions)
			ret.push_back(g.second);

		return ret;
	}

	std::string Module::getModuleName()
	{
		return this->moduleName;
	}

	void Module::setModuleName(std::string name)
	{
		this->moduleName = name;
	}
}







