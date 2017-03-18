// MiscCodegen.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "codegen.h"
#include "semantic.h"

using namespace Ast;
using namespace Codegen;


static FunctionTree* getFuncTree(CodegenInstance* cgi)
{
	FunctionTree* ftree = cgi->getCurrentFuncTree();

	iceAssert(ftree);
	return ftree;
}

static void addTypeToFuncTree(CodegenInstance* cgi, Expr* type, std::string name, TypeKind tk)
{
	FunctionTree* ftree = getFuncTree(cgi);
	ftree->types[name] = { 0, { type, tk } };
}

static void addOpOverloadToFuncTree(CodegenInstance* cgi, OpOverload* oo)
{
	FunctionTree* ftree = getFuncTree(cgi);
	ftree->operators.push_back(oo);
}

static void addFuncDeclToFuncTree(CodegenInstance* cgi, FuncDecl* decl)
{
	FunctionTree* ftree = getFuncTree(cgi);
	decl->ident.scope = cgi->getFullScope();
	ftree->funcs.push_back(FuncDefPair(0, decl, 0));
}


static void addGenericFuncToFuncTree(CodegenInstance* cgi, Func* fn)
{
	FunctionTree* ftree = getFuncTree(cgi);
	ftree->genericFunctions.push_back({ fn->decl, fn });
}


static void addProtocolToFuncTree(CodegenInstance* cgi, ProtocolDef* prot)
{
	FunctionTree* ftree = getFuncTree(cgi);

	if(ftree->protocols.find(prot->ident.name) != ftree->protocols.end())
		error(prot, "Duplicate protocol '%s'", prot->ident.name.c_str());

	ftree->protocols[prot->ident.name] = prot;
}







template <typename T>
static void fixExprScope(T* expr, std::vector<std::string> ns)
{
	expr->ident.scope = ns;
}

static size_t __counter = 0;
static void handleNestedFunc(CodegenInstance* cgi, Func* fn, std::vector<std::string> ns);
static void _handleBlock(CodegenInstance* cgi, BracedBlock* bb, std::vector<std::string> ns)
{
	for(auto e : bb->statements)
	{
		if(ForeignFuncDecl* ffi = dynamic_cast<ForeignFuncDecl*>(e))
		{
			fixExprScope(ffi->decl, ns);
		}
		else if(Func* f = dynamic_cast<Func*>(e))
		{
			handleNestedFunc(cgi, f, ns);
			if(f->decl && f->decl->genericTypes.size() > 0)
				addGenericFuncToFuncTree(cgi, f);
		}
		else if(BreakableBracedBlock* bbb = dynamic_cast<BreakableBracedBlock*>(e))
		{
			ns.push_back("__anon_scope_" + std::to_string(__counter++));
			_handleBlock(cgi, bbb->body, ns);
			ns.pop_back();
		}
		else if(IfStmt* i = dynamic_cast<IfStmt*>(e))
		{
			for(auto c : i->cases)
			{
				ns.push_back("__anon_scope_" + std::to_string(__counter++));
				_handleBlock(cgi, std::get<1>(c), ns);
				ns.pop_back();
			}

			if(i->final)
			{
				ns.push_back("__anon_scope_" + std::to_string(__counter++));
				_handleBlock(cgi, i->final, ns);
				ns.pop_back();
			}
		}
	}
}

static void handleNestedFunc(CodegenInstance* cgi, Func* fn, std::vector<std::string> ns)
{
	fixExprScope(fn->decl, ns);

	ns.push_back(fn->decl->ident.name);
	cgi->pushNamespaceScope(fn->decl->ident.name);

	_handleBlock(cgi, fn->block, ns);

	cgi->popNamespaceScope();
	ns.pop_back();
}

template <typename T>
static void handleNestedType(CodegenInstance* cgi, T* expr, std::vector<std::string> ns)
{
	fixExprScope(expr, ns);

	ns.push_back(expr->ident.name);
	for(auto t : expr->nestedTypes)
	{
		fixExprScope(t.first, ns);
		handleNestedType(cgi, t.first, ns);
	}

	if(ClassDef* cls = dynamic_cast<ClassDef*>(expr))
	{
		for(auto oo : cls->operatorOverloads)
			handleNestedFunc(cgi, oo->func, ns);

		for(auto fn : cls->funcs)
			handleNestedFunc(cgi, fn, ns);

		for(auto cp : cls->cprops)
		{
			ns.push_back(cp->ident.name);

			if(cp->getter)
				_handleBlock(cgi, cp->getter, ns);

			if(cp->setter)
				_handleBlock(cgi, cp->setter, ns);

			ns.pop_back();
		}
	}
	ns.pop_back();
}



// N-pass system.
// there's no point counting at this stage.
static void codegenTopLevel(CodegenInstance* cgi, int pass, std::vector<Expr*> expressions, bool isInsideNamespace)
{
	if(pass == 0)
	{
		// add all decls and all types, and fix their scope

		auto scp = cgi->getFullScope();

		for(Expr* e : expressions)
		{
			NamespaceDecl* ns		= dynamic_cast<NamespaceDecl*>(e);
			StructDef* str			= dynamic_cast<StructDef*>(e);
			ClassDef* cls			= dynamic_cast<ClassDef*>(e);
			EnumDef* enr			= dynamic_cast<EnumDef*>(e);
			Func* fn				= dynamic_cast<Func*>(e);
			ForeignFuncDecl* ffi	= dynamic_cast<ForeignFuncDecl*>(e);
			OpOverload* oo			= dynamic_cast<OpOverload*>(e);


			if(ns)					ns->codegenPass(cgi, pass);
			else if(enr)			fixExprScope(enr, scp);
			else if(ffi)			fixExprScope(ffi->decl, scp);

			else if(str)			handleNestedType(cgi, str, scp);
			else if(cls)			handleNestedType(cgi, cls, scp);
			else if(fn)				handleNestedFunc(cgi, fn, scp);
			else if(oo)				handleNestedFunc(cgi, oo->func, scp);
		}
	}
	else if(pass == 1)
	{
		// add all the types for order-independence -- if we encounter a need, we can
		// force codegen.

		for(Expr* e : expressions)
		{
			NamespaceDecl* ns		= dynamic_cast<NamespaceDecl*>(e);
			TypeAlias* ta			= dynamic_cast<TypeAlias*>(e);
			StructDef* str			= dynamic_cast<StructDef*>(e);
			ClassDef* cls			= dynamic_cast<ClassDef*>(e);
			EnumDef* enr			= dynamic_cast<EnumDef*>(e);
			Func* fn				= dynamic_cast<Func*>(e);
			ForeignFuncDecl* ffi	= dynamic_cast<ForeignFuncDecl*>(e);
			OpOverload* oo			= dynamic_cast<OpOverload*>(e);
			ProtocolDef* prot		= dynamic_cast<ProtocolDef*>(e);

			if(ns)					ns->codegenPass(cgi, pass);
			else if(ta)				addTypeToFuncTree(cgi, ta, ta->ident.name, TypeKind::TypeAlias);
			else if(str)			addTypeToFuncTree(cgi, str, str->ident.name, TypeKind::Struct);
			else if(enr)			addTypeToFuncTree(cgi, enr, enr->ident.name, TypeKind::Enum);
			else if(ffi)			addFuncDeclToFuncTree(cgi, ffi->decl);
			else if(prot)			addProtocolToFuncTree(cgi, prot);
			else if(cls && !dynamic_cast<ExtensionDef*>(cls))
			{
				addTypeToFuncTree(cgi, cls, cls->ident.name, TypeKind::Class);
			}
			else if(oo)
			{
				addOpOverloadToFuncTree(cgi, oo);
				addFuncDeclToFuncTree(cgi, oo->func->decl);
			}
			else if(fn)
			{
				addFuncDeclToFuncTree(cgi, fn->decl);
				if(fn->decl && fn->decl->genericTypes.size() > 0)
					addGenericFuncToFuncTree(cgi, fn);
			}
		}
	}
	else if(pass == 2)
	{
		// we need the 'Type' enum to be available, as well as the 'Any' type,
		// before any variables are encountered.

		// find all protocols
		for(Expr* e : expressions)
		{
			if(ProtocolDef* prot = dynamic_cast<ProtocolDef*>(e))
				prot->createType(cgi);
		}


		if(!isInsideNamespace)
			TypeInfo::initialiseTypeInfo(cgi);
	}
	else if(pass == 3)
	{
		// pass 2: create declarations
		for(Expr* e : expressions)
		{
			StructDef* str			= dynamic_cast<StructDef*>(e);
			EnumDef* enr			= dynamic_cast<EnumDef*>(e);
			ClassDef* cls			= dynamic_cast<ClassDef*>(e);		// extension : class
			ForeignFuncDecl* ffi	= dynamic_cast<ForeignFuncDecl*>(e);
			NamespaceDecl* ns		= dynamic_cast<NamespaceDecl*>(e);

			if(ffi)					ffi->codegen(cgi);
			else if(cls)			cls->createType(cgi);
			else if(enr)			enr->createType(cgi);
			else if(str)			str->createType(cgi);
			else if(ns)				ns->codegenPass(cgi, pass);
		}
	}
	else if(pass == 4)
	{
		// pass 3: types
		for(Expr* e : expressions)
		{
			StructDef* str			= dynamic_cast<StructDef*>(e);
			EnumDef* enr			= dynamic_cast<EnumDef*>(e);
			ClassDef* cls			= dynamic_cast<ClassDef*>(e);		// extension : class
			NamespaceDecl* ns		= dynamic_cast<NamespaceDecl*>(e);

			if(str)					str->codegen(cgi);
			else if(enr)			enr->codegen(cgi);
			else if(cls)			cls->codegen(cgi);
			else if(ns)				ns->codegenPass(cgi, pass);
		}

		// generate the type info.
		// now that we have all the types that we need, and they're all fully
		// processed, we create the Type enum.
		TypeInfo::generateTypeInfo(cgi);
	}
	else if(pass == 5)
	{
		for(Expr* e : expressions)
		{
			NamespaceDecl* ns		= dynamic_cast<NamespaceDecl*>(e);
			VarDecl* vd				= dynamic_cast<VarDecl*>(e);

			if(ns)					ns->codegenPass(cgi, pass);
			else if(vd)
			{
				vd->isGlobal = true;
				vd->codegen(cgi);
			}
		}
	}
	else if(pass == 6)
	{
		// pass 4: functions.
		for(Expr* e : expressions)
		{
			Func* func				= dynamic_cast<Func*>(e);
			NamespaceDecl* ns		= dynamic_cast<NamespaceDecl*>(e);
			OpOverload* oo			= dynamic_cast<OpOverload*>(e);

			if(oo && isInsideNamespace)
				warn(oo, "Placing operator overloads inside a namespace will make them completely inaccessible.");

			if(func && !func->didCodegen)	func->codegen(cgi);
			if(oo)							oo->codegenOp(cgi, { });
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

	codegenTopLevel(cgi, pass, this->innards->statements, true);

	cgi->popNamespaceScope();
}

Result_t Root::codegen(CodegenInstance* cgi, fir::Type* extratype, fir::Value* target)
{
	// this is getting quite out of hand.
	// note: we're using <= to show that there are N passes.
	// don't usually do this.
	for(int pass = 0; pass <= 6; pass++)
		codegenTopLevel(cgi, pass, this->topLevelExpressions, false);

	// run the after-codegen checkers.
	SemAnalysis::analyseVarUsage(cgi);


	return Result_t(0, 0);
}




























