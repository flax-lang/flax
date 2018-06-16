// Module.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "gluecode.h"
#include "ir/module.h"

#include <sstream>

namespace fir
{
	Module::Module(std::string nm)
	{
		this->moduleName = nm;
	}

	GlobalVariable* Module::createGlobalVariable(const Identifier& ident, Type* type, ConstantValue* initVal, bool isImmut, LinkageType linkage)
	{
		GlobalVariable* gv = new GlobalVariable(ident, this, type, isImmut, linkage, initVal);
		if(this->globals.find(ident) != this->globals.end())
			error("Already have a global with name '%s'", ident.str());

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
			error("No such global with name '%s'", id.str());

		return this->globals[id];
	}



	GlobalVariable* Module::getOrCreateVirtualTableForClass(ClassType* cls)
	{
		if(this->vtables.find(cls) == this->vtables.end())
		{
			auto fmethods = std::vector<fir::Function*>(cls->virtualMethodCount);
			auto methods = std::vector<fir::ConstantValue*>(cls->virtualMethodCount);

			for(auto meth : cls->reverseVirtualMethodMap)
				methods[meth.first] = meth.second, fmethods[meth.first] = meth.second;

			//! ACHTUNG !
			// TODO: should we make the vtable immutable?

			auto table = ConstantArray::get(ArrayType::get(FunctionType::get({ }, Type::getVoid()), cls->virtualMethodCount), methods);
			auto vtab = this->createGlobalVariable(Identifier("__vtable_" + cls->getTypeName().mangled(), IdKind::Name),
				table->getType(), table, false, LinkageType::External);

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
			error("No such type with name '%s'", id.str());

		return this->namedTypes[id];
	}

	void Module::addNamedType(const Identifier& id, Type* type)
	{
		if(this->namedTypes.find(id) != this->namedTypes.end())
			error("Type '%s' exists already", id.str());

		this->namedTypes[id] = type;
	}











	void Module::addFunction(Function* func)
	{
		if(this->functions.find(func->getName()) != this->functions.end())
			error("Function '%s' exists already", func->getName().str());

		this->functions[func->getName()] = func;
	}

	void Module::removeFunction(Function* func)
	{
		if(this->functions.find(func->getName()) == this->functions.end())
			error("Function '%s' does not exist, cannot remove", func->getName().str());

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
		for(auto fn : this->functions)
		{
			// if(fn.first.name == id.name && fn.first.scope == id.scope)
			if(fn.first == id)
				ret.push_back(fn.second);
		}

		return ret;
	}

	Function* Module::getOrCreateFunction(const Identifier& id, FunctionType* ftype, LinkageType linkage)
	{
		if(this->functions.find(id) != this->functions.end())
		{
			if(!this->functions[id]->getType()->isTypeEqual(ftype))
			{
				error("Function '%s' redeclared with different type (have '%s', new '%s')", id.str(),
					this->functions[id]->getType(), ftype);
			}

			return this->functions[id];
		}

		Function* f = new Function(id, ftype, this, linkage);
		this->functions[id] = f;

		return f;
	}


	static size_t getAggregateSize(Module* mod, const std::vector<Type*>& tys)
	{
		size_t ptr = 0;
		size_t aln = 0;

		for(auto ty : tys)
		{
			auto a = mod->getAlignmentOfType(ty);
			if(ptr % a > 0)
				ptr += (a - (ptr % a));

			ptr += mod->getSizeOfType(ty);
			aln = std::max(aln, a);
		}

		if(ptr % aln > 0)
			ptr += (aln - (ptr % aln));

		return ptr;
	}

	size_t Module::getSizeOfType(Type* type)
	{
		auto ptrt = fir::Type::getInt8Ptr();
		auto i64t = fir::Type::getInt64();

		if(type->isVoidType())                                      return 0;
		else if(type->isBoolType())                                 return 1;
		else if(type->isPointerType() || type->isFunctionType())    return sizeof(void*);
		else if(type->isPrimitiveType())                            return type->getBitWidth() / 8;
		else if(type->isArraySliceType())                           return getAggregateSize(this, { ptrt, i64t });
		else if(type->isStringType() || type->isDynamicArrayType()) return getAggregateSize(this, { ptrt, i64t, i64t, ptrt });
		else if(type->isRangeType())                                return getAggregateSize(this, { i64t, i64t, i64t });
		else if(type->isArrayType())
		{
			return type->toArrayType()->getArraySize() * this->getSizeOfType(type->getArrayElementType());
		}
		else if(type->isEnumType())
		{
			return getAggregateSize(this, { i64t, type->toEnumType()->getCaseType() });
		}
		else if(type->isAnyType())
		{
			return getAggregateSize(this, { i64t, ptrt, fir::ArrayType::get(fir::Type::getInt8(), BUILTIN_ANY_DATA_BYTECOUNT) });
		}
		else if(type->isClassType() || type->isStructType() || type->isTupleType())
		{
			std::vector<Type*> tys;

			if(type->isClassType())
			{
				auto c = type->toClassType();
				auto base = c;
				while(base)
				{
					tys.insert(tys.begin(), base->getElements().begin(), base->getElements().end());
					base = base->getBaseClass();
				}

				tys.insert(tys.begin(), fir::Type::getInt8Ptr());
			}
			else if(type->isStructType())
			{
				tys = type->toStructType()->getElements();
			}
			else
			{
				tys = type->toTupleType()->getElements();
			}

			return getAggregateSize(this, tys);
		}
		else
		{
			error("cannot get size of unsupported type '%s'", type);
		}
	}

	size_t Module::getAlignmentOfType(Type* type)
	{
		if(type->isArrayType())     return this->getAlignmentOfType(type->getArrayElementType());
		else                        return this->getSizeOfType(type);
	}

























	GlobalVariable* Module::createGlobalString(std::string str)
	{
		static int stringId = 0;

		if(this->globalStrings.find(str) != this->globalStrings.end())
			return this->globalStrings[str];

		GlobalVariable* gs = new GlobalVariable(Identifier("static_string" + std::to_string(stringId++), IdKind::Name), this,
			Type::getInt8Ptr(), true, LinkageType::Internal, 0);

		return (this->globalStrings[str] = gs);
	}














	std::string Module::print()
	{
		std::string ret;
		ret = "# MODULE = " + this->getModuleName() + "\n";

		for(auto string : this->globalStrings)
		{
			ret += "global string (%" + std::to_string(string.second->id);

			std::string copy;
			for(auto c : string.first)
			{
				if(c == '\r') copy += "\\r";
				else if(c == '\n') copy += "\\n";
				else if(c == '\t') copy += "\\t";
				else copy += c;
			}

			ret += ") [" + std::to_string(string.first.length()) + "] = \"" + copy + "\"\n";
		}

		for(auto global : this->globals)
		{
			ret += "global " + global.first.str() + " (%" + std::to_string(global.second->id) + ") :: "
				+ global.second->getType()->getPointerElementType()->str() + "\n";
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

		for(auto fp : this->functions)
		{
			Function* ffn = fp.second;

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
				ret += strprintf("\n    arg %s (%%%zu) :: %s", arg->getName().name, arg->id, arg->getType()->str());
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





	Function* Module::getIntrinsicFunction(std::string id)
	{
		Identifier name;
		FunctionType* ft = 0;
		if(id == "memcpy")
		{
			name = Identifier("memcpy", IdKind::Name);
			ft = FunctionType::get({ fir::Type::getMutInt8Ptr(), fir::Type::getInt8Ptr(),
				fir::Type::getInt64(), fir::Type::getInt32(), fir::Type::getBool() },
				fir::Type::getVoid());
		}
		else if(id == "memmove")
		{
			name = Identifier("memmove", IdKind::Name);
			ft = FunctionType::get({ fir::Type::getMutInt8Ptr(), fir::Type::getMutInt8Ptr(),
				fir::Type::getInt64(), fir::Type::getInt32(), fir::Type::getBool() },
				fir::Type::getVoid());
		}
		else if(id == "memset")
		{
			name = Identifier("memset", IdKind::Name);
			ft = FunctionType::get({ fir::Type::getMutInt8Ptr(), fir::Type::getInt8(),
				fir::Type::getInt64(), fir::Type::getInt32(), fir::Type::getBool() },
				fir::Type::getVoid());
		}
		else if(id == "memcmp")
		{
			// note: memcmp isn't an actual llvm intrinsic, but we support it anyway
			// at llvm-translate-time, we make a function.

			name = Identifier("memcmp", IdKind::Name);
			ft = FunctionType::get({ fir::Type::getInt8Ptr(), fir::Type::getInt8Ptr(),
				fir::Type::getInt64(), fir::Type::getInt32(), fir::Type::getBool() },
				fir::Type::getInt32());
		}
		else if(id == "roundup_pow2")
		{
			// rounds up to the nearest power of 2
			// 127 -> 128
			// 1 -> 1
			// 40 -> 64

			name = Identifier("roundup_pow2", IdKind::Name);
			ft = FunctionType::get({ fir::Type::getInt64() }, fir::Type::getInt64());
		}

		if(this->intrinsicFunctions.find(name) != this->intrinsicFunctions.end())
			return this->intrinsicFunctions[name];

		this->intrinsicFunctions[name] = new Function(name, ft, this, LinkageType::Internal);
		return this->intrinsicFunctions[name];
	}








	std::vector<GlobalVariable*> Module::getGlobalVariables()
	{
		std::vector<GlobalVariable*> ret;
		for(auto g : this->globals)
			ret.push_back(g.second);

		return ret;
	}

	std::vector<Type*> Module::getNamedTypes()
	{
		std::vector<Type*> ret;
		for(auto g : this->namedTypes)
			ret.push_back(g.second);

		return ret;
	}

	std::vector<Function*> Module::getAllFunctions()
	{
		std::vector<Function*> ret;
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







