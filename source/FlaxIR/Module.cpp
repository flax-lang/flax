// Module.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ir/module.h"

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

	GlobalVariable* Module::tryGetGlobalVariable(std::string name)
	{
		if(this->globals.find(name) == this->globals.end())
			return 0;

		return this->globals[name];
	}

	GlobalVariable* Module::getGlobalVariable(std::string name)
	{
		if(this->globals.find(name) == this->globals.end())
			error("ICE: no such global with name %s", name.c_str());

		return this->globals[name];
	}

	GlobalVariable* Module::createGlobalVariable(Identifier id, Type* type, ConstantValue* initVal, bool isImmut, LinkageType linkage)
	{
		return this->createGlobalVariable(id.str(), type, initVal, isImmut, linkage);
	}

	GlobalVariable* Module::createGlobalVariable(Identifier id, Type* type, bool isImmut, LinkageType linkage)
	{
		return this->createGlobalVariable(id.str(), type, isImmut, linkage);
	}

	GlobalVariable* Module::declareGlobalVariable(Identifier id, Type* type, bool isImmut)
	{
		return this->declareGlobalVariable(id.str(), type, isImmut);
	}

	GlobalVariable* Module::tryGetGlobalVariable(Identifier id)
	{
		return this->tryGetGlobalVariable(id.str());
	}

	GlobalVariable* Module::getGlobalVariable(Identifier id)
	{
		return this->getGlobalVariable(id.str());
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

		// fprintf(stderr, "add named type %s, %s\n", name.c_str(), type->str().c_str());
		this->namedTypes[name] = type;
	}

	void Module::addExtensionType(std::string name, StructType* type)
	{
		// the target type doesn't need to exist in *this* module, which makes it slightly iffy
		this->extensionTypes[name] = type;
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

			// fprintf(stderr, "returning existing function %s (id = %zu)\n", name.c_str(), this->functions[name]->id);
			return this->functions[name];
		}

		Function* f = new Function(name, ftype, this, linkage);
		this->functions[name] = f;

		// fprintf(stderr, "returning new function %s (id = %zu) (mod %s)\n", name.c_str(), f->id, this->moduleName.c_str());
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



	void Module::declareFunction(Identifier id, FunctionType* ftype)
	{
		this->declareFunction(id.str(), ftype);
	}

	void Module::deleteFunction(Identifier id)
	{
		this->deleteFunction(id.str());
	}

	Function* Module::getFunction(Identifier id)
	{
		return this->getFunction(id.str());
	}

	Function* Module::getOrCreateFunction(Identifier id, FunctionType* ftype, LinkageType linkage)
	{
		return this->getOrCreateFunction(id.str(), ftype, linkage);
	}



























	GlobalVariable* Module::createGlobalString(std::string str)
	{
		if(this->globalStrings.find(str) != this->globalStrings.end())
			return this->globalStrings[str];

		GlobalVariable* gs = new GlobalVariable(str, this, PointerType::getInt8Ptr(), true, LinkageType::Internal, 0);
		this->globalStrings[str] = gs;

		return gs;
	}
















	std::string Module::print()
	{
		std::string ret;
		ret = "# MODULE = " + this->getModuleName() + "\n";

		for(auto string : this->globalStrings)
		{
			ret += "global string (%" + std::to_string(string.second->id);
			ret += ") [" + std::to_string(string.first.length()) + "] = \"" + string.first + "\"\n";
		}

		for(auto global : this->globals)
		{
			ret += "global " + global.first + " (%" + std::to_string(global.second->id) + ") :: "
				+ global.second->getType()->getPointerElementType()->str() + "\n";
		}

		for(auto type : this->namedTypes)
		{
			// should just automatically create it.
			auto c = new char[32];
			snprintf(c, 32, "%p", (void*) type.second);

			ret += "declare type :: " + type.second->str() + " :: <" + std::string(c) + ">\n";

			delete[] c;
		}

		for(auto fp : this->functions)
		{
			Function* ffn = fp.second;

			std::string decl;

			decl += "func: " + ffn->getName() + "(";
			for(auto a : ffn->getArguments())
			{
				decl += "%" + std::to_string(a->id) + " :: " + a->getType()->str();

				if(a != ffn->getArguments().back())
					decl += ", ";
			}

			if(ffn->blocks.size() == 0)
			{
				decl += ") -> ";
				decl += "@" + ffn->getReturnType()->str();
				decl += "\n";

				ret += "declare " + decl;
				continue;
			}

			ret += decl;

			ret += ") -> ";
			ret += "@" + ffn->getReturnType()->str();
			ret += "\n{";


			for(auto block : ffn->getBlockList())
			{
				ret += "\n    (%" + std::to_string(block->id) + ") " + block->getName() + ":\n";

				for(auto inst : block->instructions)
					ret += "        " + inst->str() + "\n";
			}
			ret += ("}\n\n");
		}

		return ret;
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







