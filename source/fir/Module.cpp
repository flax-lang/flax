// Module.cpp
// Copyright (c) 2014 - 2016, zhiayang
// Licensed under the Apache License Version 2.0.

#include "string_consts.h"

#include "ir/module.h"
#include "ir/irbuilder.h"

namespace fir
{
	Module::Module(const std::string& nm)
	{
		this->moduleName = nm;
	}


	void Module::finaliseGlobalConstructors()
	{
		auto builder = IRBuilder(this);
		auto entryfunc = this->entryFunction;

		if(!entryfunc)
		{
			// keep trying various things.
			std::vector<std::string> trymains = { "main", "_FF" + this->getModuleName() + "4main_FAv" };
			for(const auto& m : trymains)
			{
				entryfunc = this->getFunction(Identifier(m, IdKind::Name));
				if(entryfunc) break;
			}

			if(entryfunc)
				this->entryFunction = entryfunc;

			else
				error("fir: no entry point marked with '@entry', and no 'main' function; cannot compile program");
		}

		// it doesn't actually matter what the entry function is named -- we just need to insert some instructions at the beginning.
		iceAssert(entryfunc);
		{
			iceAssert(entryfunc->getBlockList().size() > 0);

			auto oldentry = entryfunc->getBlockList()[0];
			auto newentry = new IRBlock(entryfunc);
			newentry->setName("entryblock_entry");

			auto& blklist = entryfunc->getBlockList();
			blklist.insert(blklist.begin(), newentry);

			builder.setCurrentBlock(newentry);

			auto gif = this->getFunction(util::obfuscateIdentifier(strs::names::GLOBAL_INIT_FUNCTION));
			if(!gif) error("fir: failed to find global init function");

			builder.Call(gif);
			builder.UnCondBranch(oldentry);
		}
	}











	GlobalVariable* Module::createGlobalVariable(const Identifier& ident, Type* type, ConstantValue* initVal, bool isImmut, LinkageType linkage)
	{
		GlobalVariable* gv = new GlobalVariable(ident, this, type, isImmut, linkage, initVal);
		if(this->globals.find(ident) != this->globals.end())
			error("fir: already have a global with name '%s'", ident.str());

		this->globals[ident] = gv;
		return gv;
	}

	GlobalVariable* Module::createGlobalVariable(const Identifier& id, Type* type, bool isImmut, LinkageType linkage)
	{
		return this->createGlobalVariable(id, type, 0, isImmut, linkage);
	}

	GlobalVariable* Module::declareGlobalVariable(const Identifier& id, Type* type, bool isImmut)
	{
		return this->createGlobalVariable(id, type, 0, isImmut, LinkageType::External);
	}

	GlobalVariable* Module::tryGetGlobalVariable(const Identifier& id)
	{
		if(this->globals.find(id) == this->globals.end())
			return 0;

		return this->globals[id];
	}

	GlobalVariable* Module::getGlobalVariable(const Identifier& id)
	{
		if(this->globals.find(id) == this->globals.end())
			error("fir: no such global with name '%s'", id.str());

		return this->globals[id];
	}



	GlobalVariable* Module::getOrCreateVirtualTableForClass(ClassType* cls)
	{
		if(this->vtables.find(cls) == this->vtables.end())
		{
			auto fmethods = std::vector<fir::Function*>(cls->virtualMethodCount);
			auto methods = std::vector<fir::ConstantValue*>(cls->virtualMethodCount);

			for(auto meth : cls->reverseVirtualMethodMap)
			{
				methods[meth.first] = ConstantBitcast::get(meth.second, FunctionType::get({ }, Type::getVoid()));
				fmethods[meth.first] = meth.second;
			}

			//! ACHTUNG !
			// TODO: should we make the vtable immutable?

			auto table = ConstantArray::get(ArrayType::get(FunctionType::get({ }, Type::getVoid()), cls->virtualMethodCount), methods);
			auto vtab = this->createGlobalVariable(util::obfuscateIdentifier("vtable", cls->getTypeName().mangled()),
				table->getType(), table, true, LinkageType::External);

			this->vtables[cls] = { fmethods, vtab };
		}

		return this->vtables[cls].second;
	}
















	Function* Module::getEntryFunction()
	{
		return this->entryFunction;
	}

	void Module::setEntryFunction(Function* fn)
	{
		this->entryFunction = fn;
	}




	Type* Module::getNamedType(const Identifier& id)
	{
		if(this->namedTypes.find(id) == this->namedTypes.end())
			error("fir: no such type with name '%s'", id.str());

		return this->namedTypes[id];
	}

	void Module::addNamedType(const Identifier& id, Type* type)
	{
		if(this->namedTypes.find(id) != this->namedTypes.end())
			error("fir: type '%s' exists already", id.str());

		this->namedTypes[id] = type;
	}











	void Module::addFunction(Function* func)
	{
		if(this->functions.find(func->getName()) != this->functions.end())
			error("fir: function '%s' exists already", func->getName().str());

		this->functions[func->getName()] = func;
	}

	void Module::removeFunction(Function* func)
	{
		if(this->functions.find(func->getName()) == this->functions.end())
			error("fir: function '%s' does not exist, cannot remove", func->getName().str());

		this->functions.erase(func->getName());
	}


	Function* Module::declareFunction(const Identifier& id, FunctionType* ftype)
	{
		return this->getOrCreateFunction(id, ftype, fir::LinkageType::External);
	}

	Function* Module::getFunction(const Identifier& id)
	{
		if(this->functions.find(id) == this->functions.end())
			return 0;

		return this->functions[id];
	}

	std::vector<Function*> Module::getFunctionsWithName(const Identifier& id)
	{
		// todo: *very* inefficient.

		std::vector<Function*> ret;
		for(const auto& [ ident, fn ] : this->functions)
		{
			if(ident == id)
				ret.push_back(fn);
		}

		return ret;
	}

	Function* Module::getOrCreateFunction(const Identifier& id, FunctionType* ftype, LinkageType linkage)
	{
		if(this->functions.find(id) != this->functions.end())
		{
			if(!this->functions[id]->getType()->isTypeEqual(ftype))
			{
				error("fir: function '%s' redeclared with different type (have '%s', new '%s')", id.str(),
					this->functions[id]->getType(), ftype);
			}

			return this->functions[id];
		}

		Function* f = new Function(id, ftype, this, linkage);
		this->functions[id] = f;

		return f;
	}


























	GlobalVariable* Module::createGlobalString(const std::string& str)
	{
		static int stringId = 0;

		if(this->globalStrings.find(str) != this->globalStrings.end())
			return this->globalStrings[str];

		GlobalVariable* gs = new GlobalVariable(Identifier("static_string" + std::to_string(stringId++), IdKind::Name), this,
			Type::getInt8Ptr(), true, LinkageType::Internal, 0);

		gs->setKind(Value::Kind::prvalue);
		return (this->globalStrings[str] = gs);
	}














	std::string Module::print()
	{
		std::string ret;
		ret = "# MODULE = " + this->getModuleName() + "\n";

		for(const auto& [ str, gv ] : this->globalStrings)
		{
			ret += "global string (%" + std::to_string(gv->id);

			std::string copy;
			for(auto c : str)
			{
				if(c == '\r') copy += "\\r";
				else if(c == '\n') copy += "\\n";
				else if(c == '\t') copy += "\\t";
				else copy += c;
			}

			ret += ") [" + std::to_string(str.length()) + "] = \"" + copy + "\"\n";
		}

		for(auto global : this->globals)
		{
			ret += "global " + global.first.str() + " (%" + std::to_string(global.second->id) + ") :: "
				+ global.second->getType()->str() + "\n";
		}

		for(auto type : this->namedTypes)
		{
			// should just automatically create it.
			std::string tl;
			if(type.second->isStructType()) tl = fir::Type::typeListToString(type.second->toStructType()->getElements());
			else if(type.second->isClassType()) tl = fir::Type::typeListToString(type.second->toClassType()->getElements());
			else if(type.second->isTupleType()) tl = fir::Type::typeListToString(type.second->toTupleType()->getElements());


			ret += "declare type :: " + type.second->str() + " { " + tl + " }\n";
		}

		for(const auto& [ id, fp ] : this->functions)
		{
			Function* ffn = fp;

			std::string decl;

			// note: .str() already gives us the parameters
			decl += (ffn->isAlwaysInlined() ? "inline func: " : "func: ") + ffn->getName().str();

			if(ffn->blocks.size() == 0)
			{
				decl += " -> ";
				decl += ffn->getReturnType()->str();
				decl += "\n";

				ret += "declare " + decl;
				continue;
			}

			ret += decl;

			ret += " -> ";
			ret += ffn->getReturnType()->str();

			ret += "    # mangled = " + ffn->getName().mangled();

			ret += "\n{";

			// do the args
			for(auto arg : ffn->getArguments())
			{
				ret += strprintf("\n    arg %s (%%%d) :: %s", arg->getName().name, arg->id, arg->getType()->str());
			}


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





	Function* Module::getIntrinsicFunction(const std::string& id)
	{
		Identifier name;
		FunctionType* ft = 0;
		if(id == "memcpy")
		{
			name = Identifier("memcpy", IdKind::Name);
			ft = FunctionType::get({ fir::Type::getMutInt8Ptr(), fir::Type::getInt8Ptr(),
				fir::Type::getNativeWord(), fir::Type::getBool() },
				fir::Type::getVoid());
		}
		else if(id == "memmove")
		{
			name = Identifier("memmove", IdKind::Name);
			ft = FunctionType::get({ fir::Type::getMutInt8Ptr(), fir::Type::getMutInt8Ptr(),
				fir::Type::getNativeWord(), fir::Type::getBool() },
				fir::Type::getVoid());
		}
		else if(id == "memset")
		{
			name = Identifier("memset", IdKind::Name);
			ft = FunctionType::get({ fir::Type::getMutInt8Ptr(), fir::Type::getInt8(),
				fir::Type::getNativeWord(), fir::Type::getBool() },
				fir::Type::getVoid());
		}
		else if(id == "memcmp")
		{
			// note: memcmp isn't an actual llvm intrinsic, but we support it anyway
			// at llvm-translate-time, we make a function.

			name = Identifier("memcmp", IdKind::Name);
			ft = FunctionType::get({ fir::Type::getInt8Ptr(), fir::Type::getInt8Ptr(),
				fir::Type::getNativeWord(), fir::Type::getBool() },
				fir::Type::getInt32());
		}
		else if(id == "roundup_pow2")
		{
			// rounds up to the nearest power of 2
			// 127 -> 128
			// 1 -> 1
			// 40 -> 64

			name = Identifier("roundup_pow2", IdKind::Name);
			ft = FunctionType::get({ fir::Type::getNativeWord() }, fir::Type::getNativeWord());
		}

		if(this->intrinsicFunctions.find(name) != this->intrinsicFunctions.end())
			return this->intrinsicFunctions[name];

		this->intrinsicFunctions[name] = new Function(name, ft, this, LinkageType::Internal);
		return this->intrinsicFunctions[name];
	}








	std::vector<GlobalVariable*> Module::getGlobalVariables()
	{
		std::vector<GlobalVariable*> ret;
		ret.reserve(this->globals.size());

		for(const auto& g : this->globals)
			ret.push_back(g.second);

		return ret;
	}

	std::vector<Type*> Module::getNamedTypes()
	{
		std::vector<Type*> ret;
		ret.reserve(this->namedTypes.size());

		for(const auto& g : this->namedTypes)
			ret.push_back(g.second);

		return ret;
	}

	std::vector<Function*> Module::getAllFunctions()
	{
		std::vector<Function*> ret;
		ret.reserve(this->functions.size());

		for(const auto& g : this->functions)
			ret.push_back(g.second);

		return ret;
	}

	std::string Module::getModuleName()
	{
		return this->moduleName;
	}

	void Module::setModuleName(const std::string& name)
	{
		this->moduleName = name;
	}
}







