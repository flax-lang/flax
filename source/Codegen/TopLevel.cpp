// MiscCodegen.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "../include/ast.h"
#include "../include/codegen.h"

using namespace Ast;
using namespace Codegen;


// N-pass system.
// there's no point counting at this stage.
static void codegenTopLevel(CodegenInstance* cgi, int pass, std::deque<Expr*> expressions, bool isInsideNamespace,
	std::deque<NamespaceDecl*>* nslist)
{
	if(pass == 0)
	{
		// pass 0: setup namespaces.
		for(Expr* e : expressions)
		{
			NamespaceDecl* ns		= dynamic_cast<NamespaceDecl*>(e);
			if(ns)
			{
				nslist->push_back(ns);
				ns->codegenPass(cgi, 0);
			}
		}
	}
	else if(pass == 1)
	{
		// pass 1: setup extensions
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
	else if(pass == 2)
	{
		// pass 2: create types
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
	else if(pass == 3)
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

		// step 3: generate the type info.
		// now that we have all the types that we need, and they're all fully
		// processed, we create the Type enum.
		TypeInfo::generateTypeInfo(cgi);
	}
	else if(pass == 4)
	{
		// pass 4: create declarations
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
	else if(pass == 5)
	{
		// pass 5: everything else
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
	else if(pass == 6)
	{
		// first, look into all functions. check function calls, since everything should have already been declared.
		// if we can resolve it into a generic function, then instantiate (monomorphise) the generic function
		// with concrete types.

		// fuck. super-suboptimal -- we're relying on the parser to create a list of *EVERY* function call.
		for(auto fc : cgi->rootNode->allFunctionCalls)
			cgi->tryResolveAndInstantiateGenericFunction(fc);
	}
	else if(pass == 7)
	{
		// pass 7: functions. for generic shit.
		for(Expr* e : expressions)
		{
			Func* func				= dynamic_cast<Func*>(e);
			NamespaceDecl* ns		= dynamic_cast<NamespaceDecl*>(e);

			if(func && !func->didCodegen)	func->codegen(cgi);
			if(ns)							ns->codegenPass(cgi, pass);
		}
	}
	else
	{
		error("Invalid pass number '%d'\n", pass);
	}
}

void NamespaceDecl::codegenPass(CodegenInstance* cgi, int pass)
{
	cgi->pushNamespaceScope(this->name);
	cgi->usingNamespaces.push_back(this);

	codegenTopLevel(cgi, pass, this->innards->statements, true, &this->namespaces);

	cgi->usingNamespaces.pop_back();
	cgi->popNamespaceScope();
}

Result_t Root::codegen(CodegenInstance* cgi, llvm::Value* lhsPtr, llvm::Value* rhs)
{
	// this is getting quite out of hand.
	codegenTopLevel(cgi, 0, this->topLevelExpressions, false, &this->topLevelNamespaces);
	codegenTopLevel(cgi, 1, this->topLevelExpressions, false, &this->topLevelNamespaces);
	codegenTopLevel(cgi, 2, this->topLevelExpressions, false, &this->topLevelNamespaces);
	codegenTopLevel(cgi, 3, this->topLevelExpressions, false, &this->topLevelNamespaces);
	codegenTopLevel(cgi, 4, this->topLevelExpressions, false, &this->topLevelNamespaces);
	codegenTopLevel(cgi, 5, this->topLevelExpressions, false, &this->topLevelNamespaces);
	codegenTopLevel(cgi, 6, this->topLevelExpressions, false, &this->topLevelNamespaces);
	codegenTopLevel(cgi, 7, this->topLevelExpressions, false, &this->topLevelNamespaces);

	return Result_t(0, 0);
}











