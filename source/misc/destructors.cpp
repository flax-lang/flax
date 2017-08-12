// destructors.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"

namespace ast
{
	Stmt::~Stmt() { }
	Expr::~Expr() { }
	ImportStmt::~ImportStmt() { }
	Block::~Block () { }
	FuncDefn::~FuncDefn() { }
	// ForeignFuncDefn::~ForeignFuncDefn () { }
	VarDefn::~VarDefn() { }
	TupleDecompVarDefn::~TupleDecompVarDefn() { }
	ArrayDecompVarDefn::~ArrayDecompVarDefn() { }
	TypeExpr::~TypeExpr() { }
	Ident::~Ident() { }
	BinaryOp::~BinaryOp() { }
	UnaryOp::~UnaryOp() { }
	// FunctionCall::~FunctionCall() { }
	DotOperator::~DotOperator() { }
	LitNumber::~LitNumber() { }
	LitBool::~LitBool() { }
	LitString::~LitString() { }
	LitNull::~LitNull() { }
	LitTuple::~LitTuple() { }
	TopLevelBlock::~TopLevelBlock() { }
}









