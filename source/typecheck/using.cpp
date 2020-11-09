// using.cpp
// Copyright (c) 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "errors.h"
#include "typecheck.h"

#include "ir/type.h"
#include "memorypool.h"

static void importScopeContentsIntoNewScope(sst::TypecheckState* fs, const std::vector<std::string>& sfrom,
	const std::vector<std::string>& stoParent, const std::string& name)
{
	auto parent = fs->getTreeOfScope(stoParent);

	if(auto defs = parent->getDefinitionsWithName(name); !defs.empty())
	{
		auto err = SimpleError::make(fs->loc(),
			"cannot use import scope '%s' into scope '%s' with name '%s'; one or more conflicting definitions exist",
			zfu::join(sfrom, "::"), zfu::join(stoParent, "::"),
			name);

		for(const auto& d : defs)
			err->append(SimpleError::make(MsgType::Note, d->loc, "conflicting definition here:"));

		err->postAndQuit();
	}

	// add a thing in the current scope
	auto treedef = util::pool<sst::TreeDefn>(fs->loc());
	treedef->tree = fs->getTreeOfScope(sfrom);
	treedef->tree->treeDefn = treedef;

	parent->addDefinition(name, treedef);
}



TCResult ast::UsingStmt::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	// check what kind of expression we have.
	auto user = this->expr->typecheck(fs).expr();
	if(!dcast(sst::ScopeExpr, user) && !dcast(sst::VarRef, user))
		error(this->expr, "unsupported expression on left-side of 'using' declaration");


	// check for enumerations -- we need to handle those a little specially.
	// due to the magic of good code architecture, (haha, who am i kidding, it's pure luck)
	// if the LHS is a dot operator of any kind, we'll resolve it appropriately -- getting a VarRef to an EnumDefn if it is an enum,
	// and only getting ScopeExpr if it's really a namespace reference.


	std::vector<std::string> scopes;
	if(auto vr = dcast(sst::VarRef, user); vr && dcast(sst::EnumDefn, vr->def))
	{
		auto enrd = dcast(sst::EnumDefn, vr->def);

		scopes = enrd->id.scope;
		scopes.push_back(enrd->id.name);
	}
	// uses the same 'vr' from the branch above
	else if(vr && dcast(sst::UnionDefn, vr->def))
	{
		auto unn = dcast(sst::UnionDefn, vr->def);

		scopes = unn->id.scope;
		scopes.push_back(unn->id.name);
	}
	else
	{
		if(auto se = dcast(sst::ScopeExpr, user))
			scopes = se->scope;

		else if(auto vr = dcast(sst::VarRef, user))
			scopes = { vr->name };

	}

	if(this->useAs == "_")
	{
		auto fromtree = fs->getTreeOfScope(scopes);
		auto totree = fs->stree;

		sst::addTreeToExistingTree(totree, fromtree, totree->parent, /* pubImport: */ false, /* ignoreVis: */ true);
	}
	else
	{
		importScopeContentsIntoNewScope(fs, scopes, fs->getCurrentScope(), this->useAs);
	}

	return TCResult::getDummy();
}




































