// MiscCodegen.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "codegen.h"
#include "semantic.h"

using namespace Ast;
using namespace Codegen;


static std::pair<FunctionTree*, FunctionTree*> getFuncTrees(CodegenInstance* cgi)
{
	FunctionTree* ftree = cgi->getCurrentFuncTree();
	FunctionTree* pftree = cgi->getCurrentFuncTree(0, cgi->rootNode->publicFuncTree);

	iceAssert(ftree);
	iceAssert(pftree);

	return { ftree, pftree };
}



static void addTypeToFuncTree(CodegenInstance* cgi, Expr* type, std::string name, TypeKind tk)
{
	auto p = getFuncTrees(cgi);
	FunctionTree* ftree = p.first;
	FunctionTree* pftree = p.second;

	ftree->types[name] = { 0, { type, tk } };

	if(type->attribs & Attr_VisPublic)
		pftree->types[name] = { 0, { type, tk } };
}

static void addOpOverloadToFuncTree(CodegenInstance* cgi, OpOverload* oo)
{
	auto p = getFuncTrees(cgi);
	FunctionTree* ftree = p.first;
	FunctionTree* pftree = p.second;

	ftree->operators.push_back(oo);

	if(oo->attribs & Attr_VisPublic)
		pftree->operators.push_back(oo);
}

static void addFuncDeclToFuncTree(CodegenInstance* cgi, FuncDecl* decl)
{
	auto p = getFuncTrees(cgi);
	FunctionTree* ftree = p.first;
	FunctionTree* pftree = p.second;


	if(decl->name == "main")
	{
		if(!(decl->attribs & Attr_VisPublic))
			warn(decl, "main() function should be declared as public, forcing");

		if(decl->attribs & Attr_ForceMangle)
			warn(decl, "main() function should not be name mangled, disabling @forcemangle");

		decl->attribs |= Attr_VisPublic;
		decl->isFFI = true;
	}

	ftree->funcs.push_back({ 0, decl });

	if(decl->attribs & Attr_VisPublic)
		pftree->funcs.push_back({ 0, decl });
}


// N-pass system.
// there's no point counting at this stage.
static void codegenTopLevel(CodegenInstance* cgi, int pass, std::deque<Expr*> expressions, bool isInsideNamespace)
{
	if(pass == 0)
	{
		// add all the types for order-independence -- if we encounter a need, we can
		// force codegen.

		for(Expr* e : expressions)
		{
			NamespaceDecl* ns		= dynamic_cast<NamespaceDecl*>(e);
			TypeAlias* ta			= dynamic_cast<TypeAlias*>(e);
			Struct* str				= dynamic_cast<Struct*>(e);
			Class* cls				= dynamic_cast<Class*>(e);		// enum : class, extension : class
			Func* fn				= dynamic_cast<Func*>(e);
			ForeignFuncDecl* ffi	= dynamic_cast<ForeignFuncDecl*>(e);
			OpOverload* oo			= dynamic_cast<OpOverload*>(e);

			if(ns)					ns->codegenPass(cgi, pass);
			else if(ta)				addTypeToFuncTree(cgi, ta, ta->name, TypeKind::TypeAlias);
			else if(str)			addTypeToFuncTree(cgi, str, str->name, TypeKind::Struct);
			else if(cls)			addTypeToFuncTree(cgi, cls, cls->name, TypeKind::Class);
			else if(fn)				addFuncDeclToFuncTree(cgi, fn->decl);
			else if(ffi)			addFuncDeclToFuncTree(cgi, ffi->decl);
			else if(oo)				addOpOverloadToFuncTree(cgi, oo);
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
			Class* cls				= dynamic_cast<Class*>(e);	// enums are handled, since enum : class
			NamespaceDecl* ns		= dynamic_cast<NamespaceDecl*>(e);

			if(str)					str->createType(cgi);
			if(cls)					cls->createType(cgi);
			else if(ns)				ns->codegenPass(cgi, pass);
		}
	}
	else if(pass == 3)
	{
		// pass 3: override types with any extensions
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
				// func->decl->codegen(cgi);
			}
		}
	}
	else if(pass == 5)
	{
		// start semantic analysis before any typechecking needs to happen.
		SemAnalysis::rewriteDotOperators(cgi);

		// pass 4: everything else
		for(Expr* e : expressions)
		{
			Struct* str				= dynamic_cast<Struct*>(e);
			Class* cls				= dynamic_cast<Class*>(e);		// again, enums are handled since enum : class
			Extension* ext			= dynamic_cast<Extension*>(e);
			NamespaceDecl* ns		= dynamic_cast<NamespaceDecl*>(e);
			VarDecl* vd				= dynamic_cast<VarDecl*>(e);

			if(str)					str->codegen(cgi);
			else if(cls)			cls->codegen(cgi);
			else if(ext)			ext->codegen(cgi);
			else if(ns)				ns->codegenPass(cgi, pass);
			else if(vd)				vd->isGlobal = true, vd->codegen(cgi);
		}
	}
	else if(pass == 6)
	{
		// pass 7: functions. for generic shit.
		for(Expr* e : expressions)
		{
			Func* func				= dynamic_cast<Func*>(e);
			NamespaceDecl* ns		= dynamic_cast<NamespaceDecl*>(e);
			OpOverload* oo			= dynamic_cast<OpOverload*>(e);

			if(oo && isInsideNamespace)
				warn(oo, "Placing operator overloads inside a namespace will make them completely inaccessible.");

			if(func && !func->didCodegen)	func->codegen(cgi);
			if(oo)							oo->codegen(cgi);
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
	cgi->pushNamespaceScope(this->name, true);
	cgi->usingNamespaces.push_back(this);

	codegenTopLevel(cgi, pass, this->innards->statements, true);

	cgi->usingNamespaces.pop_back();
	cgi->popNamespaceScope();
}

Result_t Root::codegen(CodegenInstance* cgi, fir::Value* lhsPtr, fir::Value* rhs)
{
	// cgi->dependencyGraph = SemAnalysis::resolveDependencyGraph(cgi, cgi->rootNode);

	// this is getting quite out of hand.
	// note: we're using <= to show that there are 6 passes.
	// don't usually do this.
	for(int pass = 0; pass <= 6; pass++)
		codegenTopLevel(cgi, pass, this->topLevelExpressions, false);

	// run the after-codegen checkers.
	SemAnalysis::analyseVarUsage(cgi);


	return Result_t(0, 0);
}




























