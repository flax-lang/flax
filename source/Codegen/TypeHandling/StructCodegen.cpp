// StructCodegen.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "ast.h"
#include "codegen.h"

using namespace Ast;
using namespace Codegen;



fir::Type* StructDef::getType(CodegenInstance* cgi, fir::Type* extratype, bool allowFail)
{
	return this->createType(cgi);
}

fir::Type* StructDef::reifyTypeUsingMapping(CodegenInstance* cgi, std::map<std::string, fir::Type*> tm)
{
	if(cgi->reifiedGenericTypes.find({ this, tm }) != cgi->reifiedGenericTypes.end())
		return cgi->reifiedGenericTypes[{ this, tm }];

	cgi->pushGenericTypeMap(tm);

	TypePair_t* _type = cgi->getType(this->ident);

	if(!_type)
		GenError::unknownSymbol(cgi, this, this->ident.str(), SymbolType::Type);

	fir::LinkageType linkageType;
	if(this->attribs & Attr_VisPublic)
	{
		linkageType = fir::LinkageType::External;
	}
	else
	{
		linkageType = fir::LinkageType::Internal;
	}

	// see if we have nested types
	for(auto nested : this->nestedTypes)
	{
		cgi->pushNestedTypeScope(this);
		cgi->pushNamespaceScope(this->ident.name);

		nested.first->codegen(cgi);

		cgi->popNamespaceScope();
		cgi->popNestedTypeScope();
	}


	fir::StructType* str = this->createdType->toStructType();
	str = str->reify(tm);

	cgi->module->addNamedType(str->getStructName(), str);

	// add the concrete type to the mapping as well.
	if(this->genericTypes.size() > 0)
		cgi->addNewType(str, this, TypeKind::Struct);


	fir::IRBlock* curblock = cgi->irb.getCurrentBlock();

	// generate initialiser
	{
		auto defaultInitId = this->ident;
		defaultInitId.kind = IdKind::AutoGenFunc;
		defaultInitId.name = "init_" + defaultInitId.name;
		defaultInitId.functionArguments = { str->getPointerTo() };

		this->defaultInitialiser = cgi->module->getOrCreateFunction(defaultInitId, fir::FunctionType::get({ str->getPointerTo() },
			fir::Type::getVoid(), false), linkageType);

		this->initFuncs.push_back(this->defaultInitialiser);

		fir::IRBlock* iblock = cgi->irb.addNewBlockInFunction("init_" + this->ident.name, this->defaultInitialiser);
		cgi->irb.setCurrentBlock(iblock);

		// create the local instance of reference to self
		fir::Value* self = this->defaultInitialiser->getArguments().front();

		for(VarDecl* var : this->members)
		{
			// not supported in structs
			iceAssert(!var->isStatic);

			fir::Value* ptr = cgi->irb.CreateGetStructMember(self, var->ident.name);

			auto r = var->initVal ? var->initVal->codegen(cgi) : Result_t(0, 0);

			var->inferType(cgi);
			var->doInitialValue(cgi, cgi->getType(var->concretisedType), r.value, r.pointer, ptr, false, r.valueKind);
		}

		cgi->irb.CreateReturnVoid();
	}




	// create memberwise initialiser
	{
		std::vector<fir::Type*> types;
		types.push_back(str->getPointerTo());
		for(auto e : str->getElements())
			types.push_back(e);


		auto memid = this->ident;
		memid.kind = IdKind::AutoGenFunc;
		memid.name = "meminit_" + memid.name;

		fir::Function* memifunc = cgi->module->getOrCreateFunction(memid,
			fir::FunctionType::get(types, fir::Type::getVoid(cgi->getContext()), false), linkageType);

		this->initFuncs.push_back(memifunc);

		fir::IRBlock* iblock = cgi->irb.addNewBlockInFunction("initialiser_" + this->ident.name, memifunc);
		cgi->irb.setCurrentBlock(iblock);

		// create the local instance of reference to self
		fir::Value* self = memifunc->getArguments().front();


		for(size_t i = 0; i < this->members.size(); i++)
		{
			fir::Value* v = memifunc->getArguments()[i + 1];

			v->setName("memberPtr_" + std::to_string(i));
			fir::Value* ptr = cgi->irb.CreateStructGEP(self, i);

			cgi->irb.CreateStore(v, ptr);
		}

		cgi->irb.CreateReturnVoid();
	}

	cgi->irb.setCurrentBlock(curblock);
	cgi->popGenericTypeStack();

	cgi->reifiedGenericTypes[{ this, tm }] = str;
	return str;
}

Result_t StructDef::codegen(CodegenInstance* cgi, fir::Type* extratype, fir::Value* target)
{
	if(!this->createdType)
		this->createType(cgi);

	if(this->genericTypes.size() == 0)
		this->reifyTypeUsingMapping(cgi, { });

	return Result_t(0, 0);
}








fir::Type* StructDef::createType(CodegenInstance* cgi)
{
	if(this->didCreateType && this->genericTypes.empty())
		return this->createdType;

	cgi->pushGenericTypeStack();
	std::vector<fir::ParametricType*> typeParams;
	if(this->genericTypes.size() > 0)
	{
		for(auto t : this->genericTypes)
		{
			auto pt = fir::ParametricType::get(t.first);
			cgi->pushGenericType(t.first, pt);
			typeParams.push_back(pt);
		}
	}


	// see if we have nested types
	for(auto nested : this->nestedTypes)
	{
		cgi->pushNestedTypeScope(this);
		cgi->pushNamespaceScope(this->ident.name);

		nested.second = nested.first->createType(cgi);

		cgi->popNamespaceScope();
		cgi->popNestedTypeScope();
	}





	std::vector<std::pair<std::string, fir::Type*>> types;


	if(cgi->isDuplicateType(this->ident))
		GenError::duplicateSymbol(cgi, this, this->ident.str(), SymbolType::Type);

	// create a bodyless struct so we can use it
	fir::StructType* str = fir::StructType::createWithoutBody(this->ident, cgi->getContext(), this->packed);
	str->addTypeParameters(typeParams);

	iceAssert(this->createdType == 0);
	cgi->addNewType(str, this, TypeKind::Struct);

	for(VarDecl* var : this->members)
	{
		var->inferType(cgi);
		fir::Type* type = var->getType(cgi);

		if(type == str)
			error(this, "Cannot have non-pointer member of type self");

		if(!var->isStatic)
		{
			types.push_back({ var->ident.name, var->getType(cgi) });
		}
	}

	str->setBody(types);

	this->didCreateType = true;
	this->createdType = str;


	cgi->popGenericTypeStack();
	return str;
}




















