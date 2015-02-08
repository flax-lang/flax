// MiscCodegen.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "../include/ast.h"
#include "../include/codegen.h"
#include "../include/llvm_all.h"

using namespace Ast;
using namespace Codegen;

Result_t Root::codegen(CodegenInstance* cgi)
{
	// pass 1: create types and function declarations
	for(Expr* e : this->topLevelExpressions)
	{
		Struct* str				= nullptr;
		ForeignFuncDecl* ffi	= nullptr;
		Func* func				= nullptr;
		NamespaceDecl* ns		= nullptr;

		if((str = dynamic_cast<Struct*>(e)))
			str->createType(cgi);

		else if((ffi = dynamic_cast<ForeignFuncDecl*>(e)))
			ffi->codegen(cgi);

		else if((ns = dynamic_cast<NamespaceDecl*>(e)))
			ns->codegen(cgi);

		else if((func = dynamic_cast<Func*>(e)))
		{
			if(func->decl->name == "main")
			{
				func->decl->attribs |= Attr_VisPublic;
				func->decl->isFFI = true;
			}

			func->decl->codegen(cgi);
		}
	}

	// we need to clear the namespaces, then re-add them

	std::deque<std::string> namespaces = cgi->namespaceStack;
	cgi->clearNamespaceScope();

	cgi->namespaceStack.push_back(namespaces[0]);

	int nestedness = 1;
	for(Expr* e : this->topLevelExpressions)
	{
		Struct* str				= nullptr;
		Func* func				= nullptr;
		NamespaceDecl* ns		= nullptr;

		if((str = dynamic_cast<Struct*>(e)))
			str->codegen(cgi);

		else if((func = dynamic_cast<Func*>(e)))
			func->codegen(cgi);

		else if((ns = dynamic_cast<NamespaceDecl*>(e)))
		{
			if(nestedness < namespaces.size())
			{
				printf("namespace: %s\n", namespaces[nestedness].c_str());
				cgi->namespaceStack.push_back(namespaces[nestedness]);
				nestedness++;
			}
		}
	}

	return Result_t(0, 0);
}

