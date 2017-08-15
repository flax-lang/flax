// misc.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "errors.h"
#include "typecheck.h"

using TCS = sst::TypecheckState;

namespace sst
{
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
}



sst::Stmt* ast::ImportStmt::typecheck(TCS* fs, fir::Type* inferred)
{
	// nothing to check??
	unexpected(this->loc, "import statement");
}

sst::Stmt* ast::Block::typecheck(TCS* fs, fir::Type* inferred)
{
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

sst::Stmt* ast::FunctionCall::typecheck(TCS* fs, fir::Type* inferred)
{
	return 0;
}

sst::Stmt* ast::DotOperator::typecheck(TCS* fs, fir::Type* inferred)
{
	return 0;
}






















