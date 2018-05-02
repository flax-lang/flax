// typecheckstate.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "errors.h"
#include "typecheck.h"

#include <deque>

namespace sst
{
	// void TypecheckState::pushLoc(const Location& l)
	// {
	// 	this->locationStack.push_back(l);
	// }

	void TypecheckState::pushLoc(ast::Stmt* stmt)
	{
		// this->pushLoc(stmt->loc);

		this->locationStack.push_back(stmt->loc);

		pushErrorLocation(stmt);
	}

	Location TypecheckState::popLoc()
	{
		popErrorLocation();

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
			error(this->loc(), "Not inside function");

		this->currentFunctionStack.pop_back();

		iceAssert(this->bodyStack.back() == BODY_FUNC);
		this->bodyStack.pop_back();
	}

	FunctionDefn* TypecheckState::getCurrentFunction()
	{
		if(this->currentFunctionStack.empty())
			error(this->loc(), "Not inside function");

		return this->currentFunctionStack.back();
	}

	bool TypecheckState::isInFunctionBody()
	{
		return this->currentFunctionStack.size() > 0 && this->bodyStack.back() == BODY_FUNC;
	}



	TypeDefn* TypecheckState::getCurrentStructBody()
	{
		if(this->structBodyStack.empty())
			error(this->loc(), "Not inside struct body");

		return this->structBodyStack.back();
	}

	bool TypecheckState::isInStructBody()
	{
		return this->structBodyStack.size() > 0 && this->bodyStack.back() == BODY_STRUCT;
	}

	void TypecheckState::enterStructBody(TypeDefn* str)
	{
		this->structBodyStack.push_back(str);
		this->bodyStack.push_back(BODY_STRUCT);
	}

	void TypecheckState::leaveStructBody()
	{
		if(this->structBodyStack.empty())
			error(this->loc(), "Not inside struct body");

		this->structBodyStack.pop_back();

		iceAssert(this->bodyStack.back() == BODY_STRUCT);
		this->bodyStack.pop_back();
	}




	void TypecheckState::pushTree(const std::string& name)
	{
		iceAssert(this->stree);

		if(auto it = this->stree->subtrees.find(name); it != this->stree->subtrees.end())
		{
			this->stree = it->second;
		}
		else
		{
			auto newtree = new StateTree(name, this->stree->topLevelFilename, this->stree);
			this->stree->subtrees[name] = newtree;
			this->stree = newtree;
		}

		// if(!this->locationStack.empty())
		// 	info(this->loc(), "enter namespace %s in %s", name, this->stree->parent->name);
	}

	StateTree* TypecheckState::popTree()
	{
		iceAssert(this->stree);
		auto ret = this->stree;
		this->stree = this->stree->parent;

		return ret;
	}

	StateTree* TypecheckState::recursivelyFindTreeUpwards(const std::string& name)
	{
		StateTree* tree = this->stree;

		iceAssert(tree);
		while(tree)
		{
			if(tree->name == name)
				return tree;

			else if(auto it = tree->subtrees.find(name); it != tree->subtrees.end())
				return it->second;

			tree = tree->parent;
		}

		return 0;
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
		else if(scope.size() == 1)
		{
			auto s = scope[0];
			if(tree->subtrees[s] == 0)
				error(this->loc(), "No such tree '%s' in scope '%s' (in teleportation to '%s')", s, tree->name, util::serialiseScope(scope));

			return tree->subtrees[s];
		}



		for(size_t i = 1; i < scope.size(); i++)
		{
			auto s = scope[i];

			if(tree->subtrees[s] == 0)
				error(this->loc(), "No such tree '%s' in scope '%s' (in teleportation to '%s')", s, tree->name, util::serialiseScope(scope));

			tree = tree->subtrees[s];
		}

		return tree;
	}

	void TypecheckState::teleportToScope(const std::vector<std::string>& scope)
	{
		this->stree = this->getTreeOfScope(scope);
	}


	std::unordered_map<std::string, std::vector<Defn*>> StateTree::getAllDefinitions()
	{
		std::unordered_map<std::string, std::vector<Defn*>> ret;
		for(auto srcs : this->definitions)
			ret.insert(srcs.second.begin(), srcs.second.end());

		return ret;
	}

	std::vector<Defn*> StateTree::getDefinitionsWithName(const std::string& name)
	{
		std::vector<Defn*> ret;
		for(const auto& [ filename, defnMap ] : this->definitions)
		{
			(void) filename;
			if(auto it = defnMap.find(name); it != defnMap.end())
			{
				const auto& defs = it->second;
				if(defs.size() > 0) ret.insert(ret.end(), defs.begin(), defs.end());
			}
		}

		return ret;
	}

	std::vector<ast::Parameterisable*> StateTree::getUnresolvedGenericDefnsWithName(const std::string& name)
	{
		return this->unresolvedGenericDefs[name];
	}

	void StateTree::addDefinition(const std::string& sourceFile, const std::string& name, Defn* def)
	{
		this->definitions[sourceFile][name].push_back(def);
	}

	void StateTree::addDefinition(const std::string& name, Defn* def)
	{
		this->definitions[this->topLevelFilename][name].push_back(def);
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

		// warn("No such tree '%s' in scope", name);
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

	bool TypecheckState::checkForShadowingOrConflictingDefinition(Defn* defn, std::function<bool (TypecheckState* fs, Defn* other)> doCheck,
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
				if(!didWarnAboutShadow)
				{
					didWarnAboutShadow = true;
					warn(defn, "Definition of %s '%s' shadows one or more previous definitions", defn->getKind(), defn->id.name);

					for(auto d : defs)
						info(d, "Previously defined here:");
				}
			}

			_tree = _tree->parent;
		}

		auto makeTheError = [](PrettyError* errs, Locatable* a, const std::string& n, const std::string& ak,
			const std::vector<std::pair<Locatable*, std::string>>& conflicts) {

			errs->addError(a, "Duplicate definition of '%s'", n);

			bool first = true;

			for(const auto& [ l, kind ] : conflicts)
			{
				errs->addInfo(l, "%shere%s:", first ? strprintf("Conflicting definition%s ", conflicts.size() == 1 ? "" : "s") : "and ",
					ak == kind ? "" : strprintf(" (as a %s)", kind));

				first = false;
			}
		};

		// ok, now check only the current scope
		auto defs = tree->getDefinitionsWithName(defn->id.name);

		bool didError = false;
		for(auto otherdef : defs)
		{
			bool conflicts = doCheck(this, otherdef);
			if(conflicts)
			{
				if(!didError)
				{
					PrettyError errs;
					makeTheError(&errs, defn, defn->id.name, defn->getKind(), { std::make_pair(otherdef, otherdef->getKind()) });

					// TODO: be more intelligent about when we give this informative tidbit
					if(dcast(sst::FunctionDecl, otherdef) && dcast(sst::FunctionDecl, defn))
						errs.addInfo(Location(), "Functions cannot be overloaded based on argument names alone");

					postErrorsAndQuit(errs);
				}
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
			PrettyError errs;
			if(auto fn = dcast(sst::FunctionDecl, defn))
			{
				// honestly we can't know if we will conflict with other functions.
				// filter out by kind.

				auto newgds = util::filterMap(gdefs,
					[](ast::Parameterisable* d) -> bool {
						// return dcast(sst::TypeDefn, d) != nullptr;
						return true;
					},
					[](ast::Parameterisable* d) -> std::pair<Locatable*, std::string> {
						return std::make_pair(d, d->getKind());
					}
				);

				if(newgds.size() > 0)
					makeTheError(&errs, fn, fn->id.name, fn->getKind(), newgds);
			}
			else
			{
				// assume everything conflicts, since functions are the only thing that can overload.
				makeTheError(&errs, defn, defn->id.name, defn->getKind(),
					util::map(gdefs, [](ast::Parameterisable* d) -> std::pair<Locatable*, std::string> {
						return std::make_pair(d, d->getKind());
					})
				);
			}

			if(errs.hasErrors())
				postErrorsAndQuit(errs);
		}

		if(didError)
			doTheExit();

		return false;
	}

	static size_t _anonId = 0;
	std::string TypecheckState::getAnonymousScopeName()
	{
		return "__anon_scope_" + std::to_string(_anonId++);
	}
}

























