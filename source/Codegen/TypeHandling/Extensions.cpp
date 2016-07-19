// ExtensionCodegen.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "ast.h"
#include "codegen.h"

#include "classbase.h"

using namespace Ast;
using namespace Codegen;

fir::Type* ExtensionDef::createType(CodegenInstance* cgi, std::unordered_map<std::string, fir::Type*> instantiatedGenericTypes)
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

	if(ft->extensions.find(this->ident.name) != ft->extensions.end())
	{
		ExtensionDef* ext = ft->extensions[this->ident.name];

		ext->funcs.insert(ext->funcs.end(), this->funcs.begin(), this->funcs.end());
		ext->cprops.insert(ext->cprops.end(), this->cprops.begin(), this->cprops.end());
		ext->subscriptOverloads.insert(ext->subscriptOverloads.end(), this->subscriptOverloads.begin(), this->subscriptOverloads.end());
		ext->assignmentOverloads.insert(ext->assignmentOverloads.end(), this->assignmentOverloads.begin(), this->assignmentOverloads.end());


		// doing nested type check here because it's unweidly elsewhere
		{
			std::unordered_map<std::string, StructBase*> map;
			for(auto te : ext->nestedTypes)
				map[te.first->ident.name] = te.first;

			for(auto tt : this->nestedTypes)
			{
				if(map.find(tt.first->ident.name) != map.end())
				{
					auto te = map[tt.first->ident.name];
					errorNoExit(tt.first, "Another extension already declared a nested type '%s'", te->ident.name.c_str());
					info(te, "The existing declaration is here.");
					doTheExit();
				}
			}

			ext->nestedTypes.insert(ext->nestedTypes.end(), this->nestedTypes.begin(), this->nestedTypes.end());
		}

		this->isDuplicate = true;
	}
	else
	{
		ft->extensions[this->ident.name] = this;
		if(this->attribs & Attr_VisPublic)
			cgi->getCurrentFuncTree(0, cgi->rootNode->publicFuncTree)->extensions[this->ident.name] = this;
	}

	this->didCreateType = true;

	TypePair_t* tp = cgi->getType(this->ident);
	if(!tp) error(this, "Type %s does not exist in the scope %s", this->ident.name.c_str(), this->ident.str().c_str());

	iceAssert(tp->first);
	this->createdType = tp->first->toStructType();


	return tp->first;
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

	TypePair_t* tp = cgi->getType(this->ident);
	iceAssert(tp);
	iceAssert(tp->first);
	iceAssert(tp->second.first);

	fir::StructType* fstr = tp->first->toStructType();
	StructBase* astr = dynamic_cast<StructBase*>(tp->second.first);

	iceAssert(fstr);
	iceAssert(astr);

	doCodegenForMemberFunctions(cgi, this);
	doCodegenForComputedProperties(cgi, this);

	iceAssert(astr->defaultInitialiser);
	fir::Function* defaultInit = cgi->module->getOrCreateFunction(astr->defaultInitialiser->getName(), astr->defaultInitialiser->getType(),
		astr->defaultInitialiser->linkageType);

	doCodegenForAssignmentOperators(cgi, this);
	doCodegenForSubscriptOperators(cgi, this);


	for(Func* f : this->funcs)
	{
		generateMemberFunctionBody(cgi, this, f, defaultInit);
	}


	return Result_t(0, 0);
}













































