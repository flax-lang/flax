// MiscCodegen.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "../include/ast.h"
#include "../include/codegen.h"
#include "../include/llvm_all.h"

using namespace Ast;
using namespace Codegen;


// 4-pass system.
// pass 1: struct->createType()
// pass 2: extensions->createType()		-- overrides struct bodies
// pass 3: decls->codegen()
// pass 4: all->codegen()

static void codegenTopLevel(CodegenInstance* cgi, int pass, std::deque<Expr*> expressions)
{
	if(pass == 1)
	{
		// pass 1: create struct types
		for(Expr* e : expressions)
		{
			Struct* str				= dynamic_cast<Struct*>(e);
			NamespaceDecl* ns		= dynamic_cast<NamespaceDecl*>(e);

			if(str)					str->createType(cgi);
			else if(ns)				ns->codegenPass(cgi, pass);
		}
	}
	else if(pass == 2)
	{
		// pass 2: override struct types with any extensions
		for(Expr* e : expressions)
		{
			Extension* ext			= dynamic_cast<Extension*>(e);
			NamespaceDecl* ns		= dynamic_cast<NamespaceDecl*>(e);

			if(ext)					ext->createType(cgi);
			else if(ns)				ns->codegenPass(cgi, pass);
		}
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
			Extension* ext			= dynamic_cast<Extension*>(e);
			Func* func				= dynamic_cast<Func*>(e);
			NamespaceDecl* ns		= dynamic_cast<NamespaceDecl*>(e);

			if(str)					str->codegen(cgi);
			else if(ext)			ext->codegen(cgi);
			else if(func)			func->codegen(cgi);
			else if(ns)				ns->codegenPass(cgi, pass);
		}
	}
	else
	{
		error("Invalid pass number '%d'\n", pass);
	}
}

void NamespaceDecl::codegenPass(CodegenInstance* cgi, int pass)
{
	for(std::string s : this->name)
		cgi->pushNamespaceScope(s);

	codegenTopLevel(cgi, pass, this->innards->statements);

	for(std::string s : this->name)
		cgi->popNamespaceScope();
}

Result_t Root::codegen(CodegenInstance* cgi, llvm::Value* lhsPtr)
{
	codegenTopLevel(cgi, 1, this->topLevelExpressions);
	codegenTopLevel(cgi, 2, this->topLevelExpressions);
	codegenTopLevel(cgi, 3, this->topLevelExpressions);
	codegenTopLevel(cgi, 4, this->topLevelExpressions);

	return Result_t(0, 0);
}
