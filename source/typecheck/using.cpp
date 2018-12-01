// using.cpp
// Copyright (c) 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "errors.h"
#include "typecheck.h"

#include "ir/type.h"
#include "mpool.h"

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


	if(auto vr = dcast(sst::VarRef, user); vr && dcast(sst::EnumDefn, vr->def))
	{
		auto enrd = dcast(sst::EnumDefn, vr->def);

		auto scopes = enrd->id.scope;
		scopes.push_back(enrd->id.name);

		if(this->useAs == "_")
			fs->importScopeContentsIntoExistingScope(scopes, fs->stree->getScope());

		else
			fs->importScopeContentsIntoNewScope(scopes, fs->getCurrentScope(), this->useAs);
	}
	// uses the same 'vr' from the branch above
	else if(vr && dcast(sst::UnionDefn, vr->def))
	{
		auto unn = dcast(sst::UnionDefn, vr->def);

		auto scopes = unn->id.scope;
		scopes.push_back(unn->id.name);

		if(this->useAs == "_")
			fs->importScopeContentsIntoExistingScope(scopes, fs->stree->getScope());

		else
			fs->importScopeContentsIntoNewScope(scopes, fs->getCurrentScope(), this->useAs);
	}
	else
	{
		std::vector<std::string> scopes;
		if(auto se = dcast(sst::ScopeExpr, user))
			scopes = se->scope;

		else if(auto vr = dcast(sst::VarRef, user))
			scopes = { vr->name };


		if(this->useAs == "_")
			fs->importScopeContentsIntoExistingScope(scopes, fs->stree->getScope());

		else
			fs->importScopeContentsIntoNewScope(scopes, fs->getCurrentScope(), this->useAs);
	}


	return TCResult::getDummy();
}




namespace sst
{
	void TypecheckState::importScopeContentsIntoExistingScope(const std::vector<std::string>& sfrom, const std::vector<std::string>& sto)
	{
		std::function<void (sst::StateTree* from, sst::StateTree* to)> recursivelyCopyTreeContents = [&](sst::StateTree* from, sst::StateTree* to) -> void {
			for(const auto& ds : from->getAllDefinitions())
			{
				for(auto d : ds.second)
				{
					//* note: the lambda we pass in can assume that the names are the same, because we only call it when we get things of the same name.
					auto conflicts = this->checkForShadowingOrConflictingDefinition(d, [&d](sst::TypecheckState* fs, sst::Defn* other) -> bool {
						if(auto fn = dcast(sst::FunctionDecl, other); fn && dcast(sst::FunctionDecl, d))
						{
							auto fn1 = dcast(sst::FunctionDecl, d);
							iceAssert(fn1);

							return fs->isDuplicateOverload(fn->params, fn1->params);
						}
						else
						{
							// other things always conflict
							return true;
						}
					});

					if(conflicts) error("conflicts");

					// else.
					to->addDefinition(ds.first, d);
				}
			}

			for(const auto& subs : from->subtrees)
			{
				auto n = subs.first;

				if(!to->subtrees[n])
					to->subtrees[n] = new sst::StateTree(n, from->topLevelFilename, to);

				recursivelyCopyTreeContents(from->subtrees[n], to->subtrees[n]);
			}
		};

		auto fromtree = this->getTreeOfScope(sfrom);
		auto totree = this->getTreeOfScope(sto);

		recursivelyCopyTreeContents(fromtree, totree);
	}

	void TypecheckState::importScopeContentsIntoNewScope(const std::vector<std::string>& sfrom, const std::vector<std::string>& stoParent,
		const std::string& name)
	{
		auto parent = this->getTreeOfScope(stoParent);

		if(auto defs = parent->getDefinitionsWithName(name); !defs.empty())
		{
			auto err = SimpleError::make(this->loc(), "cannot use import scope '%s' into scope '%s' with name '%s'; one or more conflicting definitions exist",
				util::serialiseScope(sfrom), util::serialiseScope(stoParent), name);

			for(const auto& d : defs)
				err->append(SimpleError::make(MsgType::Note, d->loc, "conflicting definition here:"));

			err->postAndQuit();
		}

		// add a thing in the current scope
		auto treedef = util::pool<sst::TreeDefn>(this->loc());
		treedef->tree = this->getTreeOfScope(sfrom);

		parent->addDefinition(name, treedef);
	}
}





































