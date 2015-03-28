// ExtensionCodegen.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "../include/ast.h"
#include "../include/codegen.h"
#include "../include/llvm_all.h"

using namespace Ast;
using namespace Codegen;

Result_t Extension::codegen(CodegenInstance* cgi, llvm::Value* lhsPtr, llvm::Value* rhs)
{
	return Result_t(0, 0);
}

llvm::Function* Extension::createAutomaticInitialiser(CodegenInstance* cgi, llvm::StructType* stype, int extIndex)
{
	// generate initialiser
	llvm::Function* defaultInitFunc = llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getVoidTy(llvm::getGlobalContext()), llvm::PointerType::get(stype, 0), false), llvm::Function::ExternalLinkage,
		"__automatic_init#" + this->mangledName + ".ext" + std::to_string(extIndex), cgi->mainModule);

	cgi->addFunctionToScope(defaultInitFunc->getName(), FuncPair_t(defaultInitFunc, 0));
	llvm::BasicBlock* iblock = llvm::BasicBlock::Create(llvm::getGlobalContext(), "initialiser", defaultInitFunc);

	llvm::BasicBlock* oldIP = cgi->mainBuilder.GetInsertBlock();
	cgi->mainBuilder.SetInsertPoint(iblock);

	// create the local instance of reference to self
	llvm::Value* self = defaultInitFunc->arg_begin();
	self->setName("self");

	int memberBeginOffset = stype->getNumElements() - this->members.size();
	for(VarDecl* var : this->members)
	{
		assert(this->nameMap.find(var->name) != this->nameMap.end());
		int i = memberBeginOffset + this->nameMap[var->name];
		assert(i >= 0);

		llvm::Value* ptr = cgi->mainBuilder.CreateStructGEP(self, i, "memberPtr_" + var->name);

		auto r = var->initVal ? var->initVal->codegen(cgi).result : ValPtr_t(0, 0);
		var->doInitialValue(cgi, cgi->getType(var->type), r.first, r.second, ptr, false);
	}

	cgi->mainBuilder.CreateRetVoid();
	llvm::verifyFunction(*defaultInitFunc);

	cgi->mainBuilder.SetInsertPoint(oldIP);
	return defaultInitFunc;
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
			func->decl->parentStruct = str;

			std::string mangled = cgi->mangleName(func->decl->name, func->decl->params);
			if(this->nameMap.find(mangled) != this->nameMap.end())
				error(func, "Duplicate member '%s'", func->decl->name.c_str());

			str->funcs.push_back(func);
		}


		int beginOffset = str->members.size();
		for(auto p : this->nameMap)
		{
			if(str->nameMap.find(p.first) != str->nameMap.end())
				error(this, "Duplicate member '%s' in extension", p.first.c_str());

			str->nameMap[p.first] = beginOffset + p.second;
		}

		for(VarDecl* var : this->members)
		{
			llvm::Type* type = cgi->getLlvmType(var);
			if(type == existing)
				error(var, "Cannot have non-pointer member of type self");

			types[this->nameMap[var->name]] = cgi->getLlvmType(var);
		}

		for(ComputedProperty* c : this->cprops)
			str->cprops.push_back(c);

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

		// finally, override the type here
		existingtp->first = newType;

		this->didCreateType = true;
		this->scope = cgi->namespaceStack;

		str->extensions.push_back(this);
		delete types;
	}
}



