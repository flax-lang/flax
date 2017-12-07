// misc.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "errors.h"
#include "typecheck.h"

#include <deque>

using TCS = sst::TypecheckState;

namespace sst
{
	void TypecheckState::pushLoc(const Location& l)
	{
		this->locationStack.push_back(l);
	}

	void TypecheckState::pushLoc(ast::Stmt* stmt)
	{
		this->locationStack.push_back(stmt->loc);
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


	void TypecheckState::enterFunctionBody(FunctionDefn* fn)
	{
		this->currentFunctionStack.push_back(fn);
	}

	void TypecheckState::leaveFunctionBody()
	{
		if(this->currentFunctionStack.empty())
			error(this->loc(), "Not inside function");

		this->currentFunctionStack.pop_back();
	}

	FunctionDefn* TypecheckState::getCurrentFunction()
	{
		if(this->currentFunctionStack.empty())
			error(this->loc(), "Not inside function");

		return this->currentFunctionStack.back();
	}

	bool TypecheckState::isInFunctionBody()
	{
		return this->currentFunctionStack.size() > 0;
	}



	void TypecheckState::pushTree(std::string name)
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

	StateTree* TypecheckState::recursivelyFindTreeUpwards(std::string name)
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

	void TypecheckState::teleportToScope(const std::vector<std::string>& scope)
	{
		StateTree* tree = this->stree;
		while(tree->parent)
			tree = tree->parent;

		// ok, we should be at the topmost level now
		iceAssert(tree);

		for(size_t i = 1; i < scope.size(); i++)
		{
			auto s = scope[i];

			if(tree->subtrees[s] == 0)
				error(this->loc(), "No such tree '%s' in scope '%s' (in teleportation to '%s')", s, tree->name, util::serialiseScope(scope));

			tree = tree->subtrees[s];
		}

		this->stree = tree;
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
		for(auto srcs : this->definitions)
		{
			auto defs = srcs.second[name];

			if(defs.size() > 0) ret.insert(ret.end(), defs.begin(), defs.end());
		}

		return ret;
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







	std::vector<Defn*> TypecheckState::getDefinitionsWithName(std::string name, StateTree* tree)
	{
		if(tree == 0)
			tree = this->stree;

		std::vector<Defn*> ret;

		iceAssert(tree);
		while(tree)
		{
			auto fns = tree->getDefinitionsWithName(name);
			ret.insert(ret.end(), fns.begin(), fns.end());

			tree = tree->parent;
		}

		return ret;
	}

	bool TypecheckState::checkForShadowingOrConflictingDefinition(Defn* defn, std::string kind,
		std::function<bool (TypecheckState* fs, Defn* other)> doCheck, StateTree* tree)
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
					warn(defn, "Definition of %s '%s' shadows one or more previous definitions", kind, defn->id.name);
				}

				for(auto d : defs)
					info(d, "Previously defined here:");
			}

			_tree = _tree->parent;
		}

		// ok, now check only the current scope
		auto defs = tree->getDefinitionsWithName(defn->id.name);

		bool didError = false;
		for(auto def : defs)
		{
			bool conflicts = doCheck(this, def);
			if(conflicts)
			{
				if(!didError)
				{
					didError = true;
					exitless_error(defn, "Duplicate definition of %s '%s'", kind, defn->id.name);
				}
				info(def, "Conflicting definition here:");
			}
		}

		if(didError)
			doTheExit();

		return false;
	}

	static size_t _anonId = 0;
	std::string TypecheckState::getAnonymousScopeName()
	{
		// warn(this->loc(), "make anon scope %zu", _anonId);
		return "__anon_scope_" + std::to_string(_anonId++);
	}
}



sst::Expr* ast::TypeExpr::typecheck(TCS* fs, fir::Type* inferred)
{
	auto ret = new sst::TypeExpr(this->loc, fs->convertParserTypeToFIR(this->type));
	return ret;
}

sst::Stmt* ast::ImportStmt::typecheck(TCS* fs, fir::Type* inferred)
{
	// nothing to check??
	unexpected(this->loc, "import statement");
}

sst::Stmt* ast::TupleDecompVarDefn::typecheck(TCS* fs, fir::Type* inferred)
{
	return 0;
}

sst::Stmt* ast::ArrayDecompVarDefn::typecheck(TCS* fs, fir::Type* inferred)
{
	return 0;
}






















