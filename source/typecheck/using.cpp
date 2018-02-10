// using.cpp
// Copyright (c) 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "errors.h"
#include "typecheck.h"

#include "ir/type.h"

sst::Stmt* ast::UsingStmt::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	// check what kind of expression we have.
	auto user = this->expr->typecheck(fs);
	if(auto enumdef = dcast(sst::EnumDefn, user))
	{
		error("enums not supported yet");
	}
	else if(!dcast(sst::ScopeExpr, user) && !dcast(sst::VarRef, user))
	{
		error(this->expr, "Unsupported expression on left-side of 'using' declaration");
	}

	std::vector<std::string> scopes;
	if(auto se = dcast(sst::ScopeExpr, user))
	{
		scopes = se->scope;
	}
	else if(auto vr = dcast(sst::VarRef, user))
	{
		scopes = { vr->name };
	}


	auto restore = fs->getCurrentScope();
	fs->teleportToScope(scopes);

	auto tree = fs->stree;

	// add a thing in the current scope?
	auto treedef = new sst::TreeDefn(this->loc);
	treedef->tree = tree;

	fs->teleportToScope(restore);

	fs->stree->addDefinition(this->useAs, treedef);

	return new sst::DummyStmt(this->loc);
}















