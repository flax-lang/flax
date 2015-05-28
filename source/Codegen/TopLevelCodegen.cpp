// MiscCodegen.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "../include/ast.h"
#include "../include/codegen.h"
#include "../include/llvm_all.h"

using namespace Ast;
using namespace Codegen;


// 4-pass system.
// pass 0: set up the mangled names for extensions so we can reference them later
// pass 1: struct->createType()
// pass 2: extensions->createType()		-- overrides struct bodies
// pass 3: decls->codegen()
// pass 4: all->codegen()

static void codegenTopLevel(CodegenInstance* cgi, int pass, std::deque<Expr*> expressions, bool isInsideNamespace)
{
	if(pass == 0)
	{
		// pass 0: setup extensions
		for(Expr* e : expressions)
		{
			Extension* ext			= dynamic_cast<Extension*>(e);
			NamespaceDecl* ns		= dynamic_cast<NamespaceDecl*>(e);

			if(ext)					ext->mangledName = cgi->mangleWithNamespace(ext->name);
			else if(ns)				ns->codegenPass(cgi, pass);
		}

		// we need the 'Type' enum to be available, as well as the 'Any' type,
		// before any variables are encountered.

		if(!isInsideNamespace)
			TypeInfo::initialiseTypeInfo(cgi);
	}
	else if(pass == 1)
	{
		// pass 1: create types
		for(Expr* e : expressions)
		{
			Struct* str				= dynamic_cast<Struct*>(e);
			Enumeration* enr		= dynamic_cast<Enumeration*>(e);
			NamespaceDecl* ns		= dynamic_cast<NamespaceDecl*>(e);

			if(str)					str->createType(cgi);
			if(enr)					enr->createType(cgi);
			else if(ns)				ns->codegenPass(cgi, pass);
		}
	}
	else if(pass == 2)
	{
		// pass 2: override types with any extensions
		for(Expr* e : expressions)
		{
			Extension* ext			= dynamic_cast<Extension*>(e);
			NamespaceDecl* ns		= dynamic_cast<NamespaceDecl*>(e);
			TypeAlias* ta			= dynamic_cast<TypeAlias*>(e);

			if(ext)					ext->createType(cgi);
			else if(ta)				ta->createType(cgi);
			else if(ns)				ns->codegenPass(cgi, pass);
		}

		// step 2: generate the type info.
		// now that we have all the types that we need, and they're all fully
		// processed, we create the Type enum.
		TypeInfo::generateTypeInfo(cgi);
	}
	else if(pass == 3)
	{
		// pass 3: create declarations
		for(Expr* e : expressions)
		{
			ForeignFuncDecl* ffi	= dynamic_cast<ForeignFuncDecl*>(e);
			Func* func				= dynamic_cast<Func*>(e);
			NamespaceDecl* ns		= dynamic_cast<NamespaceDecl*>(e);

			if(ffi)					ffi->codegen(cgi);
			else if(ns)				ns->codegenPass(cgi, pass);
			else if(func)
			{
				if(func->decl->name == "main")
				{
					func->decl->attribs |= Attr_VisPublic;
					func->decl->isFFI = true;
				}

				func->decl->codegen(cgi);
			}
		}
	}
	else if(pass == 4)
	{
		// pass 4: everything else
		for(Expr* e : expressions)
		{
			Struct* str				= dynamic_cast<Struct*>(e);
			Enumeration* enr		= dynamic_cast<Enumeration*>(e);
			Extension* ext			= dynamic_cast<Extension*>(e);
			NamespaceDecl* ns		= dynamic_cast<NamespaceDecl*>(e);
			VarDecl* vd				= dynamic_cast<VarDecl*>(e);

			if(str)					str->codegen(cgi);
			else if(enr)			enr->codegen(cgi);
			else if(ext)			ext->codegen(cgi);
			else if(ns)				ns->codegenPass(cgi, pass);
			else if(vd)				vd->isGlobal = true, vd->codegen(cgi);
		}
	}
	else if(pass == 5)
	{
		// pass 5: functions. for generic shit.
		for(Expr* e : expressions)
		{
			Func* func				= dynamic_cast<Func*>(e);

			if(func)				func->codegen(cgi);
		}
	}
	else
	{
		error("Invalid pass number '%d'\n", pass);
	}
}

void NamespaceDecl::codegenPass(CodegenInstance* cgi, int pass)
{
	auto before = cgi->importedNamespaces;
	for(std::string s : this->name)
	{
		cgi->pushNamespaceScope(s);
		cgi->importedNamespaces.push_back(cgi->namespaceStack);
	}

	codegenTopLevel(cgi, pass, this->innards->statements, true);

	for(std::string s : this->name)
		cgi->popNamespaceScope();

	cgi->importedNamespaces = before;
}

Result_t Root::codegen(CodegenInstance* cgi, llvm::Value* lhsPtr, llvm::Value* rhs)
{
	codegenTopLevel(cgi, 0, this->topLevelExpressions, false);
	codegenTopLevel(cgi, 1, this->topLevelExpressions, false);
	codegenTopLevel(cgi, 2, this->topLevelExpressions, false);
	codegenTopLevel(cgi, 3, this->topLevelExpressions, false);
	codegenTopLevel(cgi, 4, this->topLevelExpressions, false);
	codegenTopLevel(cgi, 5, this->topLevelExpressions, false);

	return Result_t(0, 0);
}
