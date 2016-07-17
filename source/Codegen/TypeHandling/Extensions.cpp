// ExtensionCodegen.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "ast.h"
#include "codegen.h"

using namespace Ast;
using namespace Codegen;

fir::Type* ExtensionDef::createType(CodegenInstance* cgi, std::map<std::string, fir::Type*> instantiatedGenericTypes)
{
	this->ident.scope = cgi->getFullScope();

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
		ext->assignmentOverloads.insert(ext->assignmentOverloads.end(), this->assignmentOverloads.begin(), this->assignmentOverloads.end());
		ext->subscriptOverloads.insert(ext->subscriptOverloads.end(), this->subscriptOverloads.begin(), this->subscriptOverloads.end());


		for(auto te : ext->nestedTypes)
		{
			for(auto tt : this->nestedTypes)
			{
				if(te.first->ident.name == tt.first->ident.name)
				{
					errorNoExit(tt.first, "Another extension already declared a nested type '%s'", te.first->ident.name.c_str());
					info(te.first, "The existing declaration is here.");
					doTheExit();
				}
			}
		}

		ext->nestedTypes.insert(ext->nestedTypes.end(), this->nestedTypes.begin(), this->nestedTypes.end());




		for(auto ce : ext->cprops)
		{
			for(auto ct : this->cprops)
			{
				if(ce->ident.name == ct->ident.name)
				{
					errorNoExit(ct, "Another extension already declared a property '%s'", ct->ident.name.c_str());
					info(ce, "The existing declaration is here.");
					doTheExit();
				}
			}
		}

		ext->cprops.insert(ext->cprops.end(), this->cprops.begin(), this->cprops.end());


		this->isDuplicate = true;
	}
	else
	{
		ft->extensions[this->ident.name] = this;
		if(this->attribs & Attr_VisPublic)
			cgi->getCurrentFuncTree(0, cgi->rootNode->publicFuncTree)->extensions[this->ident.name] = this;
	}

	// we can't return anything useful
	return 0;
}

Result_t ExtensionDef::codegen(CodegenInstance* cgi, fir::Value* extra)
{
	if(this->isDuplicate)
		return Result_t(0, 0);



	// do the thing
	return Result_t(0, 0);
}













































