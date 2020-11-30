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


	sst::Scope scopes;
	auto vr = dcast(sst::VarRef, used);

	if(vr && dcast(sst::EnumDefn, vr->def))
	{
		auto enrd = dcast(sst::EnumDefn, vr->def);
		scopes = enrd->innerScope;
	}
	// uses the same 'vr' from the branch above
	else if(vr && dcast(sst::UnionDefn, vr->def))
	{
		auto unn = dcast(sst::UnionDefn, vr->def);
		scopes = unn->innerScope;
	}
	else
	{
		sst::TypeDefn* td = nullptr;

		// this happens in cases like `foo::bar`
		if(auto se = dcast(sst::ScopeExpr, used))
			scopes = se->scope2;

		// and this happens in cases like `foo`
		else if(vr && (td = dcast(sst::TypeDefn, vr->def)))
			scopes = td->innerScope;

		else
			error("unsupported LHS of using: '%s'", used->readableName);
	}

	if(this->useAs == "_")
	{
		sst::mergeExternalTree(this->loc, "using", fs->stree, scopes.stree);
	}
	else
	{
		// check that the current scope doesn't contain this thing
		if(auto existing = fs->getDefinitionsWithName(this->useAs); existing.size() > 0)
		{
			auto err = SimpleError::make(this->loc, "cannot use scope '%s' as '%s'; one or more conflicting definitions exist",
				scopes.string(), this->useAs);

			err->append(SimpleError::make(MsgType::Note, existing[0]->loc, "first conflicting definition here:"));
			err->postAndQuit();
		}

		auto tree = fs->stree->findOrCreateSubtree(this->useAs);
		sst::mergeExternalTree(this->loc, "using", tree, scopes.stree);
	}

	return TCResult::getDummy();
}




































