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

		{
			std::unordered_map<std::string, std::deque<FuncDecl*>> map;
			for(auto fe : ext->funcs)
				map[fe->decl->ident.name].push_back(fe->decl);

			for(auto ft : this->funcs)
			{
				if(map.find(ft->decl->ident.name) != map.end())
				{
					// check if they're the same
					for(auto efd : map[ft->decl->ident.str()])
					{
						int d = 0;
						std::deque<fir::Type*> ps;
						for(auto e : efd->params)
							ps.push_back(cgi->getExprType(e));

						if(cgi->isValidFuncOverload({ 0, ft->decl }, ps, &d, true))
						{
							errorNoExit(ft->decl, "Duplicate method delcaration: %s", ft->decl->ident.name.c_str());
							info(efd, "Previous declaration was here.");
							doTheExit();
						}
					}
				}
			}

			ext->funcs.insert(ext->funcs.end(), this->funcs.begin(), this->funcs.end());
		}


		// no choice but to O(n^2) this
		{

			ext->assignmentOverloads.insert(ext->assignmentOverloads.end(), this->assignmentOverloads.begin(), this->assignmentOverloads.end());
		}



		ext->subscriptOverloads.insert(ext->subscriptOverloads.end(), this->subscriptOverloads.begin(), this->subscriptOverloads.end());


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



		{
			std::unordered_map<std::string, ComputedProperty*> map;
			for(auto ce : ext->cprops)
				map[ce->ident.name] = ce;

			for(auto ct : this->cprops)
			{
				if(map.find(ct->ident.name) != map.end())
				{
					errorNoExit(ct, "Another extension already declared a property '%s'", ct->ident.name.c_str());
					info(map[ct->ident.name], "The existing declaration is here.");
					doTheExit();
				}
			}

			ext->cprops.insert(ext->cprops.end(), this->cprops.begin(), this->cprops.end());
		}





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













































