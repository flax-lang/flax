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
}



sst::Stmt* ast::ImportStmt::typecheck(TCS* fs, fir::Type* inferred)
{
	// nothing to check??
	unexpected(this->loc, "import statement");
}

sst::Stmt* ast::Block::typecheck(TCS* fs, fir::Type* inferred)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	auto ret = new sst::Block(this->loc);

	for(auto stmt : this->statements)
		ret->statements.push_back(stmt->typecheck(fs));

	for(auto dstmt : this->deferredStatements)
		ret->deferred.push_back(dstmt->typecheck(fs));

	return ret;
}

sst::Stmt* ast::VarDefn::typecheck(TCS* fs, fir::Type* inferred)
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

sst::Stmt* ast::TypeExpr::typecheck(TCS* fs, fir::Type* inferred)
{
	error(this->loc, "??? no way man");
}

sst::Stmt* ast::Ident::typecheck(TCS* fs, fir::Type* inferred)
{
	return 0;
}

sst::Stmt* ast::BinaryOp::typecheck(TCS* fs, fir::Type* inferred)
{
	return 0;
}

sst::Stmt* ast::UnaryOp::typecheck(TCS* fs, fir::Type* inferred)
{
	return 0;
}

sst::Stmt* ast::DotOperator::typecheck(TCS* fs, fir::Type* inferred)
{
	return 0;
}






















