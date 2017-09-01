// function.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "errors.h"
#include "typecheck.h"

#include "ir/type.h"

using TCS = sst::TypecheckState;

#define dcast(t, v)		dynamic_cast<t*>(v)


sst::Stmt* ast::FuncDefn::typecheck(TCS* fs, fir::Type* inferred)
{
	fs->pushLoc(this);
	fs->enterFunctionBody();

	defer(fs->popLoc());
	defer(fs->exitFunctionBody());

	if(this->generics.size() > 0)
	{
		fs->stree->unresolvedGenericFunctions[this->name].push_back(this);
		return 0;
	}

	using Param = sst::FunctionDefn::Param;
	auto defn = new sst::FunctionDefn(this->loc);
	std::vector<Param> ps;
	std::vector<fir::Type*> ptys;

	for(auto t : this->args)
	{
		auto p = Param { .name = t.name, .loc = t.loc, .type = fs->convertParserTypeToFIR(t.type) };
		ps.push_back(p);
		ptys.push_back(p.type);
	}

	auto retty = fs->convertParserTypeToFIR(this->returnType);

	defn->id = Identifier(this->name, IdKind::Function);
	defn->id.scope = fs->getCurrentScope();
	defn->id.params = ptys;

	defn->params = ps;
	defn->returnType = retty;
	defn->privacy = this->privacy;

	defn->isEntry = this->isEntry;
	defn->noMangle = this->noMangle;

	defn->global = !fs->isInFunctionBody();

	fs->pushTree(defn->id.mangled());

	defn->body = new sst::Block(this->body->loc);

	// do the body
	for(auto stmt : this->body->statements)
		defn->body->statements.push_back(stmt->typecheck(fs));

	for(auto stmt : this->body->deferredStatements)
		defn->body->deferred.push_back(stmt->typecheck(fs));

	fs->popTree();


	bool conflicts = fs->checkForShadowingOrConflictingDefinition(defn, "function", [defn](TCS* fs, sst::Stmt* other) -> bool {

		if(auto decl = dcast(sst::FunctionDecl, other))
		{
			// make sure we didn't fuck up somewhere
			iceAssert(decl->id.name == defn->id.name);

			// check the typelists, then
			bool ret = fir::Type::areTypeListsEqual(
				util::map(defn->params, [](Param p) -> fir::Type* { return p.type; }),
				util::map(decl->params, [](Param p) -> fir::Type* { return p.type; })
			);

			return ret;
		}
		else
		{
			// variables and functions always conflict if they're in the same namespace
			return true;
		}
	});

	if(conflicts)
		error(this, "conflicting");

	fs->stree->definitions[this->name].push_back(defn);
	return defn;
}

sst::Stmt* ast::ForeignFuncDefn::typecheck(TCS* fs, fir::Type* inferred)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	using Param = sst::FunctionDecl::Param;
	auto defn = new sst::ForeignFuncDefn(this->loc);
	std::vector<Param> ps;

	for(auto t : this->args)
		ps.push_back(Param { .name = t.name, .loc = t.loc, .type = fs->convertParserTypeToFIR(t.type) });

	auto retty = fs->convertParserTypeToFIR(this->returnType);

	defn->id = Identifier(this->name, IdKind::Name);

	defn->params = ps;
	defn->returnType = retty;
	defn->privacy = this->privacy;
	defn->isVarArg = this->isVarArg;


	bool conflicts = fs->checkForShadowingOrConflictingDefinition(defn, "function", [defn](TCS* fs, sst::Stmt* other) -> bool {

		if(auto decl = dcast(sst::FunctionDecl, other))
		{
			// make sure we didn't fuck up somewhere
			iceAssert(decl->id.name == defn->id.name);

			// check the typelists, then
			bool ret = fir::Type::areTypeListsEqual(
				util::map(defn->params, [](Param p) -> fir::Type* { return p.type; }),
				util::map(decl->params, [](Param p) -> fir::Type* { return p.type; })
			);

			return ret;
		}
		else
		{
			// variables and functions always conflict if they're in the same namespace
			return true;
		}
	});

	if(conflicts)
		error(this, "conflicting");

	fs->stree->definitions[this->name].push_back(defn);
	return defn;
}















