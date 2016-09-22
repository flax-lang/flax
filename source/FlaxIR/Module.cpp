// Module.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ir/module.h"

namespace fir
{
	Module::Module(std::string nm)
	{
		this->moduleName = nm;
	}

	GlobalVariable* Module::createGlobalVariable(Identifier ident, Type* type, ConstantValue* initVal, bool isImmut, LinkageType linkage)
	{
		GlobalVariable* gv = new GlobalVariable(ident, this, type, isImmut, linkage, initVal);
		if(this->globals.find(ident) != this->globals.end())
			error("ICE: Already have a global with name %s", ident.str().c_str());

		this->globals[ident] = gv;
		return gv;
	}

	GlobalVariable* Module::createGlobalVariable(Identifier id, Type* type, bool isImmut, LinkageType linkage)
	{
		return this->createGlobalVariable(id, type, 0, isImmut, linkage);
	}

	GlobalVariable* Module::declareGlobalVariable(Identifier id, Type* type, bool isImmut)
	{
		return this->createGlobalVariable(id, type, 0, isImmut, LinkageType::External);
	}

	GlobalVariable* Module::tryGetGlobalVariable(Identifier id)
	{
		if(this->globals.find(id) == this->globals.end())
			return 0;

		return this->globals[id];
	}

	GlobalVariable* Module::getGlobalVariable(Identifier id)
	{
		if(this->globals.find(id) == this->globals.end())
			error("ICE: no such global with name %s", id.str().c_str());

		return this->globals[id];
	}





























	Type* Module::getNamedType(Identifier id)
	{
		if(this->namedTypes.find(id) == this->namedTypes.end())
			error("ICE: no such type with name %s", id.str().c_str());

		return this->namedTypes[id];
	}

	void Module::addNamedType(Identifier id, Type* type)
	{
		if(this->namedTypes.find(id) != this->namedTypes.end())
			error("ICE: type %s exists already", id.str().c_str());

		this->namedTypes[id] = type;
	}











	void Module::addFunction(Function* func)
	{
		if(this->functions.find(func->getName()) != this->functions.end())
			error("function %s exists already", func->getName().str().c_str());

		this->functions[func->getName()] = func;
	}

	Function* Module::declareFunction(Identifier id, FunctionType* ftype)
	{
		return this->getOrCreateFunction(id, ftype, fir::LinkageType::External);
	}

	Function* Module::getFunction(Identifier id)
	{
		if(this->functions.find(id) == this->functions.end())
		{
			return 0;
		}

		return this->functions[id];
	}

	std::deque<Function*> Module::getFunctionsWithName(Identifier id)
	{
		// todo: *very* inefficient.

		std::deque<Function*> ret;
		for(auto fn : this->functions)
		{
			if(fn.first.name == id.name && fn.first.scope == id.scope)
				ret.push_back(fn.second);
		}

		return ret;
	}

	Function* Module::getOrCreateFunction(Identifier id, FunctionType* ftype, LinkageType linkage)
	{
		if(this->functions.find(id) != this->functions.end())
		{
			if(!this->functions[id]->getType()->isTypeEqual(ftype))
			{
				error("function %s redeclared with different type (have %s, new %s)", id.str().c_str(),
					this->functions[id]->getType()->cstr(), ftype->cstr());
			}

			return this->functions[id];
		}

		Function* f = new Function(id, ftype, this, linkage);
		this->functions[id] = f;

		if(f->getType()->getArgumentTypes().size() > 0 && f->getType()->getArgumentTypes()[0]->isParametricType())
		{
			info("1: %s: %s", f->getName().cstr(), f->getType()->cstr());
		}

		return f;
	}





























	GlobalVariable* Module::createGlobalString(std::string str)
	{
		static int stringId = 0;

		if(this->globalStrings.find(str) != this->globalStrings.end())
			return this->globalStrings[str];

		GlobalVariable* gs = new GlobalVariable(Identifier("static_string" + std::to_string(stringId++), IdKind::Name), this,
			PointerType::getInt8Ptr(), true, LinkageType::Internal, 0);

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
			ret += "global " + global.first.str() + " (%" + std::to_string(global.second->id) + ") :: "
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

			decl += "func: " + ffn->getName().str() + "(";
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
				ret += "\n    (%" + std::to_string(block->id) + ") " + block->getName().str() + ":\n";

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

	std::deque<Type*> Module::getNamedTypes()
	{
		std::deque<Type*> ret;
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







