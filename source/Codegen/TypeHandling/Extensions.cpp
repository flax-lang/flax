// ExtensionCodegen.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "ast.h"
#include "codegen.h"

#include "classbase.h"

using namespace Ast;
using namespace Codegen;

fir::Type* ExtensionDef::getType(CodegenInstance* cgi, bool allowFail, fir::Value* extra)
{
	if(this->createdType == 0)
		return this->createType(cgi);

	else return this->createdType;
}

fir::Type* ExtensionDef::createType(CodegenInstance* cgi)
{
	this->ident.scope = cgi->getFullScope();
	this->parentRoot = cgi->rootNode;

	// see if we have nested types
	for(auto nested : this->nestedTypes)
	{
		cgi->pushNestedTypeScope(this);
		nested.second = nested.first->createType(cgi);
		cgi->popNestedTypeScope();
	}


	for(Func* func : this->funcs)
	{
		// only override if we don't have one.
		if(this->attribs & Attr_VisPublic && !(func->decl->attribs & (Attr_VisInternal | Attr_VisPrivate | Attr_VisPublic)))
			func->decl->attribs |= Attr_VisPublic;

		func->decl->parentClass = this;
	}


	FunctionTree* ft = cgi->getCurrentFuncTree();
	{
		ft->extensions.insert(std::make_pair(this->ident.name, this));

		if(this->attribs & Attr_VisPublic)
			cgi->getCurrentFuncTree(0, cgi->rootNode->publicFuncTree)->extensions.insert(std::make_pair(this->ident.name, this));
	}

	this->didCreateType = true;

	if(cgi->getExprTypeOfBuiltin(this->ident.str()))
	{
		this->createdType = cgi->getExprTypeOfBuiltin(this->ident.str());
		return this->createdType;
	}
	else
	{
		TypePair_t* tp = cgi->getType(this->ident);

		if(!tp) error(this, "Type %s does not exist in the scope %s", this->ident.name.c_str(), this->ident.str().c_str());
		else if(tp->second.second != TypeKind::Class && tp->second.second != TypeKind::Struct)
			error(this, "Extensions can only be applied to classes or structs");

		iceAssert(tp->first);

		this->createdType = tp->first;
		return tp->first;
	}
}





Result_t ExtensionDef::codegen(CodegenInstance* cgi, fir::Value* extra)
{
	if(this->isDuplicate || this->didCodegen)
		return Result_t(0, 0);

	this->didCodegen = true;
	iceAssert(this->didCreateType);


	// do the thing
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

	TypePair_t* tp = 0;
	fir::Type* fstr = cgi->getExprTypeOfBuiltin(this->ident.str());

	if(fstr == 0)
	{
		tp = cgi->getType(this->ident);
		iceAssert(tp);
		iceAssert(tp->first);
		iceAssert(tp->second.first);

		fstr = tp->first;
	}


	iceAssert(fstr);

	doCodegenForMemberFunctions(cgi, this);
	doCodegenForComputedProperties(cgi, this);


	// only allow these funny shennanigans if we're extending a class
	fir::Function* defaultInit = 0;
	if(fstr->isClassType() || fstr->isStructType())
	{
		StructBase* astr = dynamic_cast<StructBase*>(tp->second.first);
		iceAssert(astr);

		iceAssert(astr->defaultInitialiser);
		defaultInit = cgi->module->getOrCreateFunction(astr->defaultInitialiser->getName(), astr->defaultInitialiser->getType(),
			astr->defaultInitialiser->linkageType);

		generateDeclForOperators(cgi, this);

		doCodegenForGeneralOperators(cgi, this);
		doCodegenForAssignmentOperators(cgi, this);
		doCodegenForSubscriptOperators(cgi, this);
	}


	for(Func* f : this->funcs)
	{
		if(!fstr->isClassType() && f->decl->ident.name == "init")
			error(f->decl, "Extended initialisers can only be declared on class types");

		generateMemberFunctionBody(cgi, this, f, defaultInit);
	}



	for(auto protstr : this->protocolstrs)
	{
		ProtocolDef* prot = cgi->resolveProtocolName(this, protstr);
		iceAssert(prot);

		prot->assertTypeConformity(cgi, fstr);
		this->conformedProtocols.push_back(prot);
	}




	return Result_t(0, 0);
}













































