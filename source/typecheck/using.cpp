// using.cpp
// Copyright (c) 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "defs.h"
#include "errors.h"
#include "sst.h"
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


static void importScopeContentsIntoNewScope2(sst::TypecheckState* fs, const std::vector<std::string>& sfrom,
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
	auto used = this->expr->typecheck(fs).expr();
	if(!dcast(sst::ScopeExpr, used) && !dcast(sst::VarRef, used))
		error(this->expr, "unsupported expression on left-side of 'using' declaration");


	// check for enumerations -- we need to handle those a little specially.
	// due to the magic of good code architecture, (haha, who am i kidding, it's pure luck)
	// if the LHS is a dot operator of any kind, we'll resolve it appropriately -- getting a VarRef to an EnumDefn if it is an enum,
	// and only getting ScopeExpr if it's really a namespace reference.


	sst::Scope scopes2;
	std::vector<std::string> scopes;
	auto vr = dcast(sst::VarRef, used);

	if(vr && dcast(sst::EnumDefn, vr->def))
	{
		auto enrd = dcast(sst::EnumDefn, vr->def);

		scopes = enrd->id.scope;
		scopes.push_back(enrd->id.name);

		scopes2 = enrd->innerScope;
	}
	// uses the same 'vr' from the branch above
	else if(vr && dcast(sst::UnionDefn, vr->def))
	{
		auto unn = dcast(sst::UnionDefn, vr->def);

		scopes = unn->id.scope;
		scopes.push_back(unn->id.name);

		scopes2 = unn->innerScope;
	}
	else
	{
		sst::TypeDefn* td = nullptr;

		// this happens in cases like `foo::bar`
		if(auto se = dcast(sst::ScopeExpr, used))
			scopes = se->scope, scopes2 = se->scope2;

		// and this happens in cases like `foo`
		else if(vr && (td = dcast(sst::TypeDefn, vr->def)))
			scopes = { vr->name }, scopes2 = td->innerScope;

		else
			error("what is this?");
	}

	if(this->useAs == "_")
	{
		auto fromtree = fs->getTreeOfScope(scopes);
		auto totree = fs->stree;

		sst::addTreeToExistingTree(totree, fromtree, totree->parent, /* pubImport: */ false, /* ignoreVis: */ true);
	}
	else
	{
		// check that the current scope doesn't contain this thing
		if(auto existing = fs->getDefinitionsWithName(this->useAs); existing.size() > 0)
		{
			auto err = SimpleError::make(fs->loc(), "cannot use scope '%s' as '%s'; one or more conflicting definitions exist",
				zfu::join(scopes, "::"), this->useAs);

			err->append(SimpleError::make(MsgType::Note, existing[0]->loc, "first conflicting definition here:"));
			err->postAndQuit();
		}

		auto treedef = util::pool<sst::TreeDefn>(fs->loc());
		auto tree = fs->stree->findOrCreateSubtree(this->useAs);
		treedef->tree = tree;
		treedef->tree->treeDefn = treedef;

		tree->imports.push_back(scopes2.stree);
		fs->stree->addDefinition(this->useAs, treedef);
	}

	return TCResult::getDummy();
}




































