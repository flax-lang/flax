// NamespaceCodegen.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "../include/ast.h"
#include "../include/codegen.h"
#include "../include/llvm_all.h"

using namespace Ast;
using namespace Codegen;

// recursively resolves the scope, and returns only the right-most thing (probably a function call or type ref or whatever)
static void recursiveResolveScope(ScopeResolution* _sr, CodegenInstance* cgi, std::deque<std::string>* scopes)
{
	// lhs and rhs are probably var refs
	ScopeResolution* sr	= nullptr;
	VarRef* left = nullptr;

	if((left = dynamic_cast<VarRef*>(_sr->scope)))
		scopes->push_back(left->name);

	else if((sr = dynamic_cast<ScopeResolution*>(_sr->scope)))
		recursiveResolveScope(sr, cgi, scopes);

	else
		error(_sr, "Unexpected expr type %s", typeid(*_sr->scope).name());


	VarRef* vr = nullptr;
	if((vr = dynamic_cast<VarRef*>(_sr->member)))
		scopes->push_back(vr->name);
}



static Expr* resolveScope(ScopeResolution* _sr, CodegenInstance* cgi, std::deque<std::string>* scopes)
{
	// lhs and rhs are probably var refs
	ScopeResolution* sr	= nullptr;
	VarRef* left = nullptr;

	if((left = dynamic_cast<VarRef*>(_sr->scope)))
		scopes->push_back(left->name);

	else if((sr = dynamic_cast<ScopeResolution*>(_sr->scope)))
		recursiveResolveScope(sr, cgi, scopes);

	else
		error(_sr, "Unexpected expr type %s", typeid(*_sr->scope).name());

	return _sr->member;
}



Result_t ScopeResolution::codegen(CodegenInstance* cgi)
{
	std::deque<std::string> scopes;
	Expr* result = resolveScope(this, cgi, &scopes);


	FuncCall* fc = nullptr;
	if((fc = dynamic_cast<FuncCall*>(result)))
	{
		// the funccall generator will try the pure unmangled type first
		// so we just screw with fc->name.

		fc->name = cgi->mangleWithNamespace(fc->name, scopes);
		fc->name = cgi->mangleName(fc->name, fc->params);
	}

	return result->codegen(cgi);
}


static void codegenTopLevel(CodegenInstance* cgi, int pass, std::deque<Expr*> expressions)
{
	if(pass == 1)
	{
		// pass 1: create types and function declarations
		for(Expr* e : expressions)
		{
			Struct* str				= dynamic_cast<Struct*>(e);
			ForeignFuncDecl* ffi	= dynamic_cast<ForeignFuncDecl*>(e);
			Func* func				= dynamic_cast<Func*>(e);
			NamespaceDecl* ns		= dynamic_cast<NamespaceDecl*>(e);

			if(str)
				str->createType(cgi);

			else if(ffi)
				ffi->codegen(cgi);

			else if(ns)
				ns->codegenPass1(cgi);

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
	else if(pass == 2)
	{
		for(Expr* e : expressions)
		{
			Struct* str				= dynamic_cast<Struct*>(e);
			Func* func				= dynamic_cast<Func*>(e);
			NamespaceDecl* ns		= dynamic_cast<NamespaceDecl*>(e);

			if(str)
				str->codegen(cgi);

			else if(func)
				func->codegen(cgi);

			else if(ns)
				ns->codegenPass2(cgi);
		}
	}
	else
	{
		error("Invalid pass number '%d'\n", pass);
	}
}

























void NamespaceDecl::codegenPass1(CodegenInstance* cgi)
{
	for(std::string s : this->name)
		cgi->pushNamespaceScope(s);

	codegenTopLevel(cgi, 1, this->innards->statements);

	for(std::string s : this->name)
		cgi->popNamespaceScope();
}



void NamespaceDecl::codegenPass2(CodegenInstance* cgi)
{
	for(std::string s : this->name)
		cgi->pushNamespaceScope(s);

	codegenTopLevel(cgi, 2, this->innards->statements);

	for(std::string s : this->name)
		cgi->popNamespaceScope();
}


Result_t Root::codegen(CodegenInstance* cgi)
{
	codegenTopLevel(cgi, 1, this->topLevelExpressions);
	codegenTopLevel(cgi, 2, this->topLevelExpressions);

	return Result_t(0, 0);
}







