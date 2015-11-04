// ExtensionCodegen.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "ast.h"
#include "codegen.h"

using namespace Ast;
using namespace Codegen;

Result_t Extension::codegen(CodegenInstance* cgi, fir::Value* lhsPtr, fir::Value* rhs)
{
	return Result_t(0, 0);
}

fir::Function* Extension::createAutomaticInitialiser(CodegenInstance* cgi, fir::StructType* stype, int extIndex)
{
	// generate initialiser
	fir::Function* defaultInitFunc = cgi->module->getOrCreateFunction("__auto_init__" + this->mangledName + ".ext" + std::to_string(extIndex), fir::FunctionType::get({ stype->getPointerTo() }, fir::PrimitiveType::getVoid(cgi->getContext()), false),
		fir::LinkageType::External);

	{
		VarDecl* fakeSelf = new VarDecl(this->pin, "self", true);
		fakeSelf->type = this->name + "*";

		FuncDecl* fd = new FuncDecl(this->pin, defaultInitFunc->getName(), { fakeSelf }, "Void");

		cgi->addFunctionToScope({ defaultInitFunc, fd });
	}

	fir::IRBlock* iblock = cgi->builder.addNewBlockInFunction("initialiser" + this->name, defaultInitFunc);

	fir::IRBlock* oldIP = cgi->builder.getCurrentBlock();
	cgi->builder.setCurrentBlock(iblock);

	// create the local instance of reference to self
	fir::Value* self = defaultInitFunc->getArguments().front();
	self->setName("self");

	int memberBeginOffset = stype->getElementCount() - this->members.size();
	for(VarDecl* var : this->members)
	{
		iceAssert(this->nameMap.find(var->name) != this->nameMap.end());
		int i = memberBeginOffset + this->nameMap[var->name];
		iceAssert(i >= 0);

		fir::Value* ptr = cgi->builder.CreateStructGEP(self, i);

		auto r = var->initVal ? var->initVal->codegen(cgi).result : ValPtr_t(0, 0);
		var->doInitialValue(cgi, cgi->getType(var->type.strType), r.first, r.second, ptr, false);
	}

	cgi->builder.CreateReturnVoid();
	// fir::verifyFunction(*defaultInitFunc);

	cgi->builder.setCurrentBlock(oldIP);
	return defaultInitFunc;
}

fir::Type* Extension::createType(CodegenInstance* cgi, std::map<std::string, fir::Type*> instantiatedGenericTypes)
{
	if(!cgi->isDuplicateType(this->name))
		error(this, "Cannot create extension for non-existent type '%s'", this->name.c_str());

	this->mangledName = cgi->mangleWithNamespace(this->name);

	// grab the existing type
	TypePair_t* existingtp = cgi->getType(this->mangledName);
	iceAssert(existingtp);

	fir::StructType* existing = dynamic_cast<fir::StructType*>(existingtp->first);
	cgi->module->deleteNamedType(existing->getStructName());

	if(!dynamic_cast<Class*>(existingtp->second.first))
		error(this, "Extensions can only be applied onto classes");

	Class* str = (Class*) existingtp->second.first;

	fir::Type** types = new fir::Type*[str->members.size() + this->members.size()];

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
			func->decl->parentClass = str;

			std::string mangled = cgi->mangleFunctionName(func->decl->name, func->decl->params);
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
			fir::Type* type = cgi->getExprType(var);
			if(type == existing)
				error(var, "Cannot have non-pointer member of type self");

			types[this->nameMap[var->name]] = cgi->getExprType(var);
		}

		for(ComputedProperty* c : this->cprops)
			str->cprops.push_back(c);

		std::vector<fir::Type*> vec;
		for(unsigned int i = 0; i < existing->getElementCount(); i++)
			vec.push_back(existing->getElementN(i));

		for(size_t i = 0; i < this->members.size(); i++)
			vec.push_back(types[i]);

		// first, delete the existing struct.
		existing->deleteType();

		// then, create a new type with the old name.
		fir::StructType* newType = fir::StructType::createNamed(this->mangledName, vec, cgi->getContext(), true);

		// finally, override the type here
		existingtp->first = newType;

		this->didCreateType = true;
		this->scope = cgi->namespaceStack;

		str->extensions.push_back(this);
		delete types;


		cgi->module->addNamedType(newType->getStructName(), newType);
	}

	return existingtp->first;
}



