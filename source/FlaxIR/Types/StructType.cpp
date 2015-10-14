// Type.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include <map>
#include <unordered_map>

#include "../include/codegen.h"
#include "../include/compiler.h"
#include "../include/ir/type.h"

#include "llvm/IR/Module.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/LLVMContext.h"

namespace fir
{
	// structs
	StructType::StructType(std::string name, std::deque<Type*> mems, bool islit, bool ispacked)
		: Type(islit ? FTypeKind::LiteralStruct : FTypeKind::NamedStruct)
	{
		this->structName = name;
		this->structMembers = mems;
		this->isTypePacked = ispacked;
	}

	StructType* StructType::getOrCreateNamedStruct(std::string name, std::deque<Type*> members, FTContext* tc, bool packed)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		StructType* type = new StructType(name, members, false, packed);

		// special: need to check if new type has the same name
		for(auto t : tc->typeCache[0])
		{
			if(t->isStructType() && t->toStructType()->isNamedStruct() && t->toStructType()->getStructName() == name)
			{
				// check members.
				if(!areTypeListsEqual(members, t->toStructType()->structMembers))
				{
					std::string mstr = typeListToString(members);
					error("Conflicting types for named struct %s:\n%s vs %s", name.c_str(), type->str().c_str(), mstr.c_str());
				}

				// ok.
				break;
			}
		}

		return dynamic_cast<StructType*>(tc->normaliseType(type));
	}

	StructType* StructType::getOrCreateNamedStruct(std::string name, std::vector<Type*> members, FTContext* tc, bool packed)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		std::deque<Type*> dmems;
		for(auto m : members)
			dmems.push_back(m);

		return StructType::getOrCreateNamedStruct(name, dmems, tc, packed);
	}

	StructType* StructType::getOrCreateNamedStruct(std::string name, std::initializer_list<Type*> members, FTContext* tc, bool packed)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		std::deque<Type*> dmems;
		for(auto m : members)
			dmems.push_back(m);

		return StructType::getOrCreateNamedStruct(name, dmems, tc, packed);
	}



	StructType* StructType::getLiteralStruct(std::deque<Type*> members, FTContext* tc, bool packed)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		iceAssert(members.size() > 0 && "literal struct must have body at init");

		StructType* type = new StructType("__LITERAL_STRUCT__", members, true, packed);
		return dynamic_cast<StructType*>(tc->normaliseType(type));
	}

	StructType* StructType::getLiteralStruct(std::vector<Type*> members, FTContext* tc, bool packed)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		iceAssert(members.size() > 0 && "literal struct must have body at init");

		std::deque<Type*> dmems;
		for(auto m : members)
			dmems.push_back(m);

		return StructType::getLiteralStruct(dmems, tc, packed);
	}

	StructType* StructType::getLiteralStruct(std::initializer_list<Type*> members, FTContext* tc, bool packed)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		iceAssert(members.size() > 0 && "literal struct must have body at init");

		std::deque<Type*> dmems;
		for(auto m : members)
			dmems.push_back(m);

		return StructType::getLiteralStruct(dmems, tc, packed);
	}






	// various
	std::string StructType::str()
	{
		if(this->isNamedStruct())
		{
			return this->structName + " = " + typeListToString(this->structMembers);
		}
		else if(this->isLiteralStruct())
		{
			return typeListToString(this->structMembers);
		}
		else
		{
			iceAssert(0);
		}
	}


	bool StructType::isTypeEqual(Type* other)
	{
		StructType* os = dynamic_cast<StructType*>(other);
		if(!os) return false;
		if(this->typeKind != os->typeKind) return false;
		if(this->isTypePacked != os->isTypePacked) return false;
		if(this->structMembers.size() != os->structMembers.size()) return false;
		if(this->isLiteralStruct() != os->isLiteralStruct()) return false;

		// compare names
		if(!this->isLiteralStruct() && (this->structName != os->structName)) return false;

		for(size_t i = 0; i < this->structMembers.size(); i++)
		{
			if(!this->structMembers[i]->isTypeEqual(os->structMembers[i]))
				return false;
		}

		return true;
	}


	fir::Type* StructType::getLlvmType(FTContext* tc)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		if(this->llvmType == 0)
		{
			std::vector<fir::Type*> lmems;

			for(auto m : this->structMembers)
				lmems.push_back(m->getLlvmType());

			if(this->typeKind == FTypeKind::NamedStruct)
			{
				if(!(this->llvmType = tc->module->getTypeByName(this->structName)))
					this->llvmType = fir::StructType::create(*tc->llvmContext, lmems, this->structName, this->isTypePacked);
			}
			else
			{
				this->llvmType = fir::StructType::get(*tc->llvmContext, lmems, this->isTypePacked);
			}
		}

		return this->llvmType;
	}

	// struct stuff
	std::string StructType::getStructName()
	{
		iceAssert(this->typeKind == FTypeKind::NamedStruct && "not named struct");
		return this->structName;
	}

	size_t StructType::getElementCount()
	{
		iceAssert(this->typeKind == FTypeKind::NamedStruct || this->typeKind == FTypeKind::LiteralStruct && "not struct type");
		return this->structMembers.size();
	}

	Type* StructType::getElementN(size_t n)
	{
		iceAssert(this->typeKind == FTypeKind::NamedStruct || this->typeKind == FTypeKind::LiteralStruct && "not struct type");
		iceAssert(n < this->structMembers.size() && "out of bounds");

		return this->structMembers[n];
	}

	std::vector<Type*> StructType::getElements()
	{
		iceAssert(this->typeKind == FTypeKind::NamedStruct || this->typeKind == FTypeKind::LiteralStruct && "not struct type");

		std::vector<Type*> vmems;
		for(auto m : this->structMembers)
			vmems.push_back(m);

		return vmems;
	}


	void StructType::setBody(std::initializer_list<Type*> members)
	{
		iceAssert(this->typeKind == FTypeKind::NamedStruct || this->typeKind == FTypeKind::LiteralStruct && "not struct type");

		this->structMembers = members;

		std::vector<fir::Type*> lmems;
		for(auto m : this->structMembers)
			lmems.push_back(m->getLlvmType());

		fir::cast<fir::StructType>(this->getLlvmType())->setBody(lmems, this->isPackedStruct());
	}

	void StructType::setBody(std::vector<Type*> members)
	{
		iceAssert(this->typeKind == FTypeKind::NamedStruct || this->typeKind == FTypeKind::LiteralStruct && "not struct type");

		std::deque<Type*> dmems;
		for(auto m : members)
			dmems.push_back(m);

		this->structMembers = dmems;

		std::vector<fir::Type*> lmems;
		for(auto m : this->structMembers)
			lmems.push_back(m->getLlvmType());

		fir::cast<fir::StructType>(this->getLlvmType())->setBody(lmems, this->isPackedStruct());
	}

	void StructType::setBody(std::deque<Type*> members)
	{
		iceAssert(this->typeKind == FTypeKind::NamedStruct || this->typeKind == FTypeKind::LiteralStruct && "not struct type");

		this->structMembers = members;

		std::vector<fir::Type*> lmems;
		for(auto m : this->structMembers)
			lmems.push_back(m->getLlvmType());

		fir::cast<fir::StructType>(this->getLlvmType())->setBody(lmems, this->isPackedStruct());
	}


	void StructType::deleteType(FTContext* tc)
	{
		if(!tc) tc = getDefaultFTContext();
		iceAssert(tc && "null type context");

		if(this->typeKind != FTypeKind::NamedStruct) return;

		for(auto it = tc->typeCache[0].begin(); it != tc->typeCache[0].end(); it++)
		{
			if(this->isTypeEqual(*it))
			{
				tc->typeCache[0].erase(it);
				break;
			}
		}

		// erase llvm.
		fir::StructType* st = fir::cast<fir::StructType>(this->getLlvmType());
		iceAssert(st);

		// According to llvm source Type.cpp, doing this removes the struct def from the symbol table.
		st->setName("");

		// todo: safe?
		delete this;
	}
}


















