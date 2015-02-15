// ExtensionCodegen.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "../include/ast.h"
#include "../include/codegen.h"
#include "../include/llvm_all.h"

using namespace Ast;
using namespace Codegen;

Result_t Extension::codegen(CodegenInstance* cgi, llvm::Value* lhsPtr)
{
	return Result_t(0, 0);
}

void Extension::createType(CodegenInstance* cgi)
{
	if(!cgi->isDuplicateType(this->name))
		error(this, "Cannot create extension for non-existent type '%s'", this->name.c_str());

	this->mangledName = cgi->mangleWithNamespace(this->name);

	// grab the existing type
	TypePair_t* existingtp = cgi->getType(this->mangledName);
	assert(existingtp);

	llvm::StructType* existing = llvm::cast<llvm::StructType>(existingtp->first);
	Struct* str = (Struct*) existingtp->second.first;

	llvm::Type** types = new llvm::Type*[str->members.size() + this->members.size()];

	if(!this->didCreateType)
	{
		// because we can't (and don't want to) mangle names in the parser,
		// we could only build an incomplete name -> index map
		// finish it here.

		for(auto p : this->opOverloads)
		{
			p->codegen(cgi);
		}

		for(Func* func : this->funcs)
		{
			func->decl->parentStruct = this;

			std::string mangled = cgi->mangleName(func->decl->name, func->decl->params);
			if(this->nameMap.find(mangled) != this->nameMap.end())
				error(func, "Duplicate member '%s'", func->decl->name.c_str());
		}

		for(VarDecl* var : this->members)
		{
			llvm::Type* type = cgi->getLlvmType(var);
			if(type == existing)
				error(var, "Cannot have non-pointer member of type self");

			if(str->nameMap.find(var->name) != str->nameMap.end())
				error(var, "Duplicate member '%s' in extension", var->name.c_str());

			int beginOffset = str->members.size();
			for(auto p : this->nameMap)
			{
				str->nameMap[p.first] = beginOffset + p.second;
			}

			types[this->nameMap[var->name]] = cgi->getLlvmType(var);
		}

		std::vector<llvm::Type*> vec;
		for(unsigned int i = 0; i < existing->getStructNumElements(); i++)
			vec.push_back(existing->getElementType(i));

		for(size_t i = 0; i < this->members.size(); i++)
			vec.push_back(types[i]);

		// first, delete the existing struct. do this by calling setName(""). According to llvm source Type.cpp,
		// doing this removes the struct def from the symbol table.
		existing->setName("");

		// then, create a new type with the old name.
		llvm::StructType* newType = llvm::StructType::create(cgi->getContext(), this->mangledName);
		newType->setBody(vec, existing->isPacked());

		existingtp->first = newType;

		// // next, erase the type that we had (in our own type table)
		// cgi->removeType(this->mangledName);

		// // add the type again
		// cgi->addNewType(newType, (Struct*) existingtp->second.first, ExprType::Struct);

		this->didCreateType = true;
		this->scope = cgi->namespaceStack;
		delete types;
	}
}



