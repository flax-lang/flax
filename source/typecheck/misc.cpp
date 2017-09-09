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


	void TypecheckState::enterFunctionBody()
	{
		this->functionNestLevel++;
	}

	void TypecheckState::exitFunctionBody()
	{
		this->functionNestLevel--;
	}

	bool TypecheckState::isInFunctionBody()
	{
		return this->functionNestLevel > 0;
	}



	void TypecheckState::pushTree(std::string name)
	{
		iceAssert(this->stree);

		auto newtree = new StateTree(name, this->stree);
		this->stree->subtrees[name] = newtree;
		this->stree = newtree;
	}

	StateTree* TypecheckState::popTree()
	{
		iceAssert(this->stree);
		auto ret = this->stree;
		this->stree = this->stree->parent;

		return ret;
	}


	std::string TypecheckState::serialiseCurrentScope()
	{
		std::deque<std::string> scope;
		auto tree = this->stree;

		while(tree)
		{
			scope.push_front(tree->name);
			tree = tree->parent;
		}

		std::string ret;
		for(auto s : scope)
			ret += s + ".";

		if(!ret.empty() && ret.back() == '.')
			ret.pop_back();

		return ret;
	}

	std::vector<std::string> TypecheckState::getCurrentScope()
	{
		std::deque<std::string> scope;
		auto tree = this->stree;

		while(tree)
		{
			scope.push_front(tree->name);
			tree = tree->parent;
		}

		return std::vector<std::string>(scope.begin(), scope.end());
	}

	std::vector<Stmt*> TypecheckState::getDefinitionsWithName(std::string name, StateTree* tree)
	{
		if(tree == 0)
			tree = this->stree;

		std::vector<Stmt*> ret;

		iceAssert(tree);
		while(tree)
		{
			auto fns = tree->definitions[name];
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
			if(auto defs = _tree->definitions[defn->id.name]; defs.size() > 0)
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
		auto defs = tree->definitions[defn->id.name];

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

sst::Expr* ast::RangeExpr::typecheck(sst::TypecheckState* fs, fir::Type* inferred)
{
	return 0;
}

sst::Expr* ast::SliceOp::typecheck(sst::TypecheckState* fs, fir::Type* inferred)
{
	return 0;
}

sst::Stmt* ast::TupleDecompVarDefn::typecheck(TCS* fs, fir::Type* inferred)
{
	return 0;
}

sst::Stmt* ast::ArrayDecompVarDefn::typecheck(TCS* fs, fir::Type* inferred)
{
	return 0;
}

sst::Expr* ast::DotOperator::typecheck(TCS* fs, fir::Type* inferred)
{
	return 0;
}






















