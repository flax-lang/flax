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

	if(this->useAs == "_")
	{
		// we need to copy all the definitions from 'tree' into the current stree.
		// but, while doing so we need to check for duplicates having the same name.
		// this will get messy.

		std::function<void (sst::StateTree* from, sst::StateTree* to)> recursivelyCopyTreeContents = [&](sst::StateTree* from, sst::StateTree* to) -> void {
			for(const auto& ds : from->getAllDefinitions())
			{
				for(auto d : ds.second)
				{
					//* note: the lambda we pass in can assume that the names are the same, because we only call it when we get things of the same name.
					auto conflicts = fs->checkForShadowingOrConflictingDefinition(d, "definition", [&d](sst::TypecheckState* fs, sst::Defn* other) -> bool {
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
					to->addDefinition(d->id.name, d);
				}
			}

			for(const auto& subs : from->subtrees)
			{
				auto n = subs.first;

				if(!to->subtrees[n])
					to->subtrees[n] = new sst::StateTree(n, from->topLevelFilename, from);

				recursivelyCopyTreeContents(from->subtrees[n], to->subtrees[n]);
			}
		};

		recursivelyCopyTreeContents(tree, fs->stree);
	}
	else
	{
		if(auto defs = fs->stree->getDefinitionsWithName(this->useAs); !defs.empty())
		{
			exitless_error(this, "Cannot use scope '%s' as '%s' in current scope; one or more conflicting definitions exist");
			for(const auto& d : defs)
				info(d, "Conflicting definition here:");

			doTheExit();
		}

		fs->stree->addDefinition(this->useAs, treedef);
	}

	return new sst::DummyStmt(this->loc);
}




namespace sst
{
	void importScopeContentsIntoAnotherScope(const std::vector<std::string>& from, const std::vector<std::string>& to)
	{
	}
}





































