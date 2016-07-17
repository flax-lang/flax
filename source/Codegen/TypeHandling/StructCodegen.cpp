// StructCodegen.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "ast.h"
#include "codegen.h"

using namespace Ast;
using namespace Codegen;


Result_t StructDef::codegen(CodegenInstance* cgi, fir::Value* extra)
{
	this->createType(cgi);

	iceAssert(this->didCreateType);
	TypePair_t* _type = cgi->getType(this->ident);

	if(!_type)
		GenError::unknownSymbol(cgi, this, this->ident.str(), SymbolType::Type);


	// if we're already done, don't.
	if(this->didCodegen)
		return Result_t(0, 0);

	this->didCodegen = true;





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
		nested.first->codegen(cgi);
		cgi->popNestedTypeScope();
	}



	fir::StructType* str = this->createdType;
	cgi->module->addNamedType(str->getStructName(), str);


	fir::IRBlock* curblock = cgi->builder.getCurrentBlock();

	// generate initialiser
	{
		auto defaultInitId = this->ident;
		defaultInitId.kind = IdKind::AutoGenFunc;
		defaultInitId.name = "init_" + defaultInitId.name;
		defaultInitId.functionArguments = { str->getPointerTo() };

		fir::Function* defifunc = cgi->module->getOrCreateFunction(defaultInitId, fir::FunctionType::get({ str->getPointerTo() },
			fir::PrimitiveType::getVoid(), false), linkageType);

		this->initFuncs.push_back(defifunc);

		fir::IRBlock* iblock = cgi->builder.addNewBlockInFunction("initialiser_" + this->ident.name, defifunc);
		cgi->builder.setCurrentBlock(iblock);

		// create the local instance of reference to self
		fir::Value* self = defifunc->getArguments().front();

		for(VarDecl* var : this->members)
		{
			// not supported in structs
			iceAssert(!var->isStatic);

			int i = this->nameMap[var->ident.name];
			iceAssert(i >= 0);

			fir::Value* ptr = cgi->builder.CreateStructGEP(self, i);

			auto r = var->initVal ? var->initVal->codegen(cgi).result : ValPtr_t(0, 0);
			var->doInitialValue(cgi, cgi->getTypeByString(var->type.strType), r.first, r.second, ptr, false);
		}

		cgi->builder.CreateReturnVoid();
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
			fir::FunctionType::get(types, fir::PrimitiveType::getVoid(cgi->getContext()), false), linkageType);

		this->initFuncs.push_back(memifunc);

		fir::IRBlock* iblock = cgi->builder.addNewBlockInFunction("initialiser_" + this->ident.name, memifunc);
		cgi->builder.setCurrentBlock(iblock);

		// create the local instance of reference to self
		fir::Value* self = memifunc->getArguments().front();


		for(size_t i = 0; i < this->members.size(); i++)
		{
			fir::Value* v = memifunc->getArguments()[i + 1];

			v->setName("memberPtr_" + std::to_string(i));
			fir::Value* ptr = cgi->builder.CreateStructGEP(self, i);

			cgi->builder.CreateStore(v, ptr);
		}

		cgi->builder.CreateReturnVoid();
	}

	cgi->builder.setCurrentBlock(curblock);


	return Result_t(0, 0);
}








fir::Type* StructDef::createType(CodegenInstance* cgi, std::map<std::string, fir::Type*> instantiatedGenericTypes)
{
	if(this->didCreateType)
		return this->createdType;

	this->ident.scope = cgi->getFullScope();



	// see if we have nested types
	for(auto nested : this->nestedTypes)
	{
		cgi->pushNestedTypeScope(this);
		nested.second = nested.first->createType(cgi);
		cgi->popNestedTypeScope();
	}




	// check our inheritances??
	fir::Type** types = new fir::Type*[this->members.size()];


	if(cgi->isDuplicateType(this->ident))
		GenError::duplicateSymbol(cgi, this, this->ident.str(), SymbolType::Type);

	// create a bodyless struct so we can use it
	fir::StructType* str = fir::StructType::createNamedWithoutBody(this->ident, cgi->getContext(), this->packed);

	iceAssert(this->createdType == 0);
	cgi->addNewType(str, this, TypeKind::Struct);



	// because we can't (and don't want to) mangle names in the parser,
	// we could only build an incomplete name -> index map
	// finish it here.

	for(VarDecl* var : this->members)
	{
		var->inferType(cgi);
		fir::Type* type = cgi->getExprType(var);
		if(type == str)
		{
			error(this, "Cannot have non-pointer member of type self");
		}

		if(!var->isStatic)
		{
			int i = this->nameMap[var->ident.name];
			iceAssert(i >= 0);

			types[i] = cgi->getExprType(var);
		}
	}






	std::vector<fir::Type*> vec(types, types + this->nameMap.size());
	str->setBody(vec);

	this->didCreateType = true;

	delete[] types;

	this->createdType = str;

	return str;
}




















