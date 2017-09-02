// destructors.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"

namespace ast
{
	Stmt::~Stmt() { }
	Expr::~Expr() { }
	Ident::~Ident() { }
	Block::~Block () { }
	VarDefn::~VarDefn() { }
	UnaryOp::~UnaryOp() { }
	LitBool::~LitBool() { }
	LitNull::~LitNull() { }
	FuncDefn::~FuncDefn() { }
	TypeExpr::~TypeExpr() { }
	AssignOp::~AssignOp() { }
	BinaryOp::~BinaryOp() { }
	LitTuple::~LitTuple() { }
	LitNumber::~LitNumber() { }
	LitString::~LitString() { }
	ImportStmt::~ImportStmt() { }
	DotOperator::~DotOperator() { }
	DeferredStmt::~DeferredStmt() { }
	FunctionCall::~FunctionCall() { }
	TopLevelBlock::~TopLevelBlock() { }
	ForeignFuncDefn::~ForeignFuncDefn () { }
	TupleDecompVarDefn::~TupleDecompVarDefn() { }
	ArrayDecompVarDefn::~ArrayDecompVarDefn() { }
}









