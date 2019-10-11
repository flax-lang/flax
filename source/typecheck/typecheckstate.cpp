// typecheckstate.cpp
// Copyright (c) 2014 - 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "errors.h"
#include "typecheck.h"

#include "ir/type.h"
#include "memorypool.h"

#include <deque>

namespace sst
{

	void TypecheckState::pushLoc(ast::Stmt* stmt)
	{
		this->locationStack.push_back(stmt->loc);
	}

	void TypecheckState::pushLoc(const Location& l)
	{
		this->locationStack.push_back(l);
	}



	Location TypecheckState::popLoc()
	{
		iceAssert(this->locationStack.size() > 0);
		auto last = this->locationStack.back();
		this->locationStack.pop_back();

		return last;
	}

	Location TypecheckState::loc()
	{
		iceAssert(this->locationStack.size() > 0);
		return this->locationStack.back();
	}

	#define BODY_FUNC       1
	#define BODY_STRUCT     2
	void TypecheckState::enterFunctionBody(FunctionDefn* fn)
	{
		this->currentFunctionStack.push_back(fn);
		this->bodyStack.push_back(BODY_FUNC);
	}

	void TypecheckState::leaveFunctionBody()
	{
		if(this->currentFunctionStack.empty())
			error(this->loc(), "not inside function");

		this->currentFunctionStack.pop_back();

		iceAssert(this->bodyStack.back() == BODY_FUNC);
		this->bodyStack.pop_back();
	}

	FunctionDefn* TypecheckState::getCurrentFunction()
	{
		if(this->currentFunctionStack.empty())
			error(this->loc(), "not inside function");

		return this->currentFunctionStack.back();
	}

	bool TypecheckState::isInFunctionBody()
	{
		return this->currentFunctionStack.size() > 0 && this->bodyStack.back() == BODY_FUNC;
	}



	fir::Type* TypecheckState::getCurrentSelfContext()
	{
		if(this->selfContextStack.empty())
			error(this->loc(), "not inside struct body");

		return this->selfContextStack.back();
	}

	bool TypecheckState::hasSelfContext()
	{
		return this->selfContextStack.size() > 0;
	}

	void TypecheckState::pushSelfContext(fir::Type* str)
	{
		this->selfContextStack.push_back(str);
		this->bodyStack.push_back(BODY_STRUCT);
	}

	void TypecheckState::popSelfContext()
	{
		if(this->selfContextStack.empty())
			error(this->loc(), "not inside struct body");

		this->selfContextStack.pop_back();

		iceAssert(this->bodyStack.back() == BODY_STRUCT);
		this->bodyStack.pop_back();
	}





	void TypecheckState::enterSubscript(Expr* arr)
	{
		this->subscriptArrayStack.push_back(arr);
	}

	Expr* TypecheckState::getCurrentSubscriptArray()
	{
		iceAssert(this->subscriptArrayStack.size() > 0);
		return this->subscriptArrayStack.back();
	}

	void TypecheckState::leaveSubscript()
	{
		iceAssert(this->subscriptArrayStack.size() > 0);
		this->subscriptArrayStack.pop_back();
	}

	bool TypecheckState::isInSubscript()
	{
		return this->subscriptArrayStack.size() > 0;
	}









	void TypecheckState::pushTree(const std::string& name, bool createAnonymously)
	{
		iceAssert(this->stree);

		if(auto it = this->stree->subtrees.find(name); it != this->stree->subtrees.end())
		{
			this->stree = it->second;
		}
		else
		{
			auto newtree = util::pool<StateTree>(name, this->stree->topLevelFilename, this->stree, createAnonymously);
			this->stree->subtrees[name] = newtree;

			// make a treedef.
			newtree->treeDefn = util::pool<TreeDefn>(Location());
			newtree->treeDefn->tree = newtree;

			this->stree = newtree;
		}
	}

	StateTree* TypecheckState::popTree()
	{
		iceAssert(this->stree);
		auto ret = this->stree;
		this->stree = this->stree->parent;

		return ret;
	}

	void TypecheckState::enterBreakableBody()
	{
		this->breakableBodyNest++;
	}

	void TypecheckState::leaveBreakableBody()
	{
		iceAssert(this->breakableBodyNest > 0);
		this->breakableBodyNest--;
	}

	bool TypecheckState::isInBreakableBody()
	{
		return this->breakableBodyNest > 0;
	}

	void TypecheckState::enterDeferBlock()
	{
		this->deferBlockNest++;
	}

	void TypecheckState::leaveDeferBlock()
	{
		iceAssert(this->deferBlockNest > 0);
		this->deferBlockNest--;
	}

	bool TypecheckState::isInDeferBlock()
	{
		return this->deferBlockNest > 0;
	}

	std::string TypecheckState::serialiseCurrentScope()
	{
		std::deque<std::string> scope;
		StateTree* tree = this->stree;

		while(tree)
		{
			scope.push_front(tree->name);
			tree = tree->parent;
		}

		return util::serialiseScope(std::vector<std::string>(scope.begin(), scope.end()));
	}

	std::vector<std::string> TypecheckState::getCurrentScope()
	{
		std::deque<std::string> scope;
		StateTree* tree = this->stree;

		while(tree)
		{
			scope.push_front(tree->name);
			tree = tree->parent;
		}

		return std::vector<std::string>(scope.begin(), scope.end());
	}

	StateTree* TypecheckState::getTreeOfScope(const std::vector<std::string>& scope)
	{
		StateTree* tree = this->stree;
		while(tree->parent)
			tree = tree->parent;

		// ok, we should be at the topmost level now
		iceAssert(tree);

		//! we're changing the behaviour subtly from how it used to function.
		//* previously, we would always skip the first 'scope', under the assumption that it would be the name of the current module anyway.
		//* however, the new behaviour is that, if the number of scopes passed in is 1 (one), we teleport directly to that scope, assuming an
		//* implied module scope.

		//* if the number of scopes passed is 0, we teleport to the top level scope (as we do now).

		// TODO: investigate if this is the right thing to do...?

		if(scope.empty())
		{
			return tree;
		}
		// else if(scope.size() == 1)
		// {
		// 	auto s = scope[0];
		// 	//* note: if our size is 1, we should check if s == toplevel_name -- if so, then we're declaring
		// 	//* things in the global scope -- which is allowed!

		// 	if(s == tree->name)
		// 		return tree;


		// 	if(auto it = tree->subtrees.find(s); it == tree->subtrees.end())
		// 	{
		// 		error(this->loc(), "no such tree '%s' in scope '%s' (in teleportation to '%s')", s, tree->name, util::serialiseScope(scope));
		// 	}
		// 	else
		// 	{
		// 		return it->second;
		// 	}
		// }


		for(size_t i = 0; i < scope.size(); i++)
		{
			auto s = scope[i];

			//* note: if our size is 1, we should check if s == toplevel_name -- if so, then we're declaring
			//* things in the global scope -- which is allowed!

			if(s == tree->name)
				continue;

			if(auto it = tree->subtrees.find(s); it == tree->subtrees.end())
			{
				error(this->loc(), "nonexistent tree '%s' in scope '%s' (in teleportation to '%s')", s, tree->name, util::serialiseScope(scope));
			}
			else
			{
				tree = it->second;
			}
		}

		return tree;
	}

	void TypecheckState::teleportToScope(const std::vector<std::string>& scope)
	{
		this->stree = this->getTreeOfScope(scope);
	}


	util::hash_map<std::string, std::vector<Defn*>> StateTree::getAllDefinitions()
	{
		util::hash_map<std::string, std::vector<Defn*>> ret;
		for(auto srcs : this->definitions)
			ret.insert(srcs.second.defns.begin(), srcs.second.defns.end());

		return ret;
	}

	std::vector<Defn*> StateTree::getDefinitionsWithName(const std::string& name)
	{
		std::vector<Defn*> ret;
		for(const auto& [ filename, defnMap ] : this->definitions)
		{
			(void) filename;
			if(auto it = defnMap.defns.find(name); it != defnMap.defns.end())
			{
				const auto& defs = it->second;
				if(defs.size() > 0) ret.insert(ret.end(), defs.begin(), defs.end());
			}
		}

		return ret;
	}

	std::vector<ast::Parameterisable*> StateTree::getUnresolvedGenericDefnsWithName(const std::string& name)
	{
		if(auto it = this->unresolvedGenericDefs.find(name); it != this->unresolvedGenericDefs.end())
			return it->second;

		else
			return { };
	}

	void StateTree::addDefinition(const std::string& sourceFile, const std::string& name, Defn* def, const TypeParamMap_t& gmaps)
	{
		// this->definitions[sourceFile][util::typeParamMapToString(name, gmaps)].push_back(def);
		this->definitions[sourceFile].defns[name].push_back(def);
	}

	void StateTree::addDefinition(const std::string& _name, Defn* def, const TypeParamMap_t& gmaps)
	{
		this->addDefinition(this->topLevelFilename, _name, def, gmaps);
	}

	// TODO: maybe cache this someday?
	std::vector<std::string> StateTree::getScope()
	{
		std::deque<std::string> ret;
		ret.push_front(this->name);

		auto tree = this->parent;
		while(tree)
		{
			ret.push_front(tree->name);
			tree = tree->parent;
		}

		return std::vector<std::string>(ret.begin(), ret.end());
	}

	StateTree* StateTree::searchForName(const std::string& name)
	{
		auto tree = this;
		while(tree)
		{
			if(tree->name == name)
				return tree;

			tree = tree->parent;
		}

		return 0;
	}









	std::vector<Defn*> TypecheckState::getDefinitionsWithName(const std::string& name, StateTree* tree)
	{
		if(tree == 0)
			tree = this->stree;

		std::vector<Defn*> ret;

		iceAssert(tree);
		while(tree)
		{
			auto fns = tree->getDefinitionsWithName(name);

			if(fns.size() > 0)
				return fns;
				// ret.insert(ret.end(), fns.begin(), fns.end());

			tree = tree->parent;
		}

		return ret;
	}

	bool TypecheckState::checkForShadowingOrConflictingDefinition(Defn* defn, std::function<bool (TypecheckState* fs, Defn* other)> conflictCheckCallback,
		StateTree* tree)
	{
		if(tree == 0)
			tree = this->stree;

		// first, check for shadowing
		bool didWarnAboutShadow = false;

		auto _tree = tree->parent;
		while(_tree)
		{
			if(auto defs = _tree->getDefinitionsWithName(defn->id.name); defs.size() > 0)
			{
				if(false && !didWarnAboutShadow)
				{
					didWarnAboutShadow = true;
					warn(defn, "definition of %s '%s' shadows one or more previous definitions", defn->getKind(), defn->id.name);

					for(auto d : defs)
						info(d, "previously defined here:");
				}
			}

			_tree = _tree->parent;
		}

		auto makeTheError = [](Locatable* a, const std::string& n, const std::string& ak,
			const std::vector<std::pair<Locatable*, std::string>>& conflicts) -> SimpleError* {

			auto err = SimpleError::make(a->loc, "duplicate definition of '%s'", n);

			bool first = true;

			for(const auto& [ l, kind ] : conflicts)
			{
				err->append(SimpleError::make(MsgType::Note, l->loc, "%shere%s:", first ? strprintf("conflicting %s ",
					util::plural("definition", conflicts.size())) : "and ", ak == kind ? "" : strprintf(" (as a %s)", kind)));

				first = false;
			}

			return err;
		};

		// ok, now check only the current scope
		auto defs = tree->getDefinitionsWithName(defn->id.name);

		for(auto otherdef : defs)
		{
			if(!dcast(TreeDefn, otherdef) && !otherdef->type->containsPlaceholders() && conflictCheckCallback(this, otherdef))
			{
				auto errs = makeTheError(defn, defn->id.name, defn->getKind(), { std::make_pair(otherdef, otherdef->getKind()) });

				// TODO: be more intelligent about when we give this informative tidbit
				if(dcast(sst::FunctionDecl, otherdef) && dcast(sst::FunctionDecl, defn))
				{
					auto a = dcast(sst::FunctionDecl, defn);
					auto b = dcast(sst::FunctionDecl, otherdef);
					if(fir::Type::areTypeListsEqual(util::map(a->params, [](const auto& p) -> fir::Type* { return p.type; }),
						util::map(b->params, [](const auto& p) -> fir::Type* { return p.type; })))
					{
						errs->append(BareError::make(MsgType::Note, "functions cannot be overloaded over argument names or"
							" return types alone"));
					}
				}

				errs->postAndQuit();
			}
		}

		// while in the interests of flexibility we provide a predicate for users to specify whether or not the duplicate definition is
		// actually conflicting, for generics i couldn't be damned.
		//? to know for certain that a definition will conflict with a generic thing, either we are:
		// A: variable & generic anything
		// B: function & generic type
		// C: type & generic anything

		if(auto gdefs = tree->getUnresolvedGenericDefnsWithName(defn->id.name); gdefs.size() > 0)
		{
			if(auto fn = dcast(sst::FunctionDecl, defn))
			{
				// honestly we can't know if we will conflict with other functions.
				// filter out by kind.

				auto newgds = util::filterMap(gdefs,
					[](ast::Parameterisable* d) -> bool {
						return dcast(ast::FuncDefn, d) == nullptr;
					},
					[](ast::Parameterisable* d) -> std::pair<Locatable*, std::string> {
						return std::make_pair(d, d->getKind());
					}
				);

				if(newgds.size() > 0)
					makeTheError(fn, fn->id.name, fn->getKind(), newgds)->postAndQuit();
			}
			else
			{
				// assume everything conflicts, since functions are the only thing that can overload.
				makeTheError(defn, defn->id.name, defn->getKind(),
					util::map(gdefs, [](ast::Parameterisable* d) -> std::pair<Locatable*, std::string> {
						return std::make_pair(d, d->getKind());
					})
				)->postAndQuit();
			}
		}

		return false;
	}

	void TypecheckState::pushAnonymousTree()
	{
		static size_t _anonId = 0;
		this->pushTree(std::to_string(_anonId++), /* createAnonymously: */ true);
	}
}

























