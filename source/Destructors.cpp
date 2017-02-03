// Destructors.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "codegen.h"

namespace Ast
{
	Func::~Func() { }
	Tuple::~Tuple() { }
	BinOp::~BinOp() { }
	Break::~Break() { }
	Alloc::~Alloc() { }
	Number::~Number() { }
	VarRef::~VarRef() { }
	Typeof::~Typeof() { }
	Return::~Return() { }
	Import::~Import() { }
	IfStmt::~IfStmt() { }
	BoolVal::~BoolVal() { }
	NullVal::~NullVal() { }
	VarDecl::~VarDecl() { }
	UnaryOp::~UnaryOp() { }
	Dealloc::~Dealloc() { }
	EnumDef::~EnumDef() { }
	FuncDecl::~FuncDecl() { }
	FuncCall::~FuncCall() { }
	Continue::~Continue() { }
	ClassDef::~ClassDef() { }
	DummyExpr::~DummyExpr() { }
	WhileLoop::~WhileLoop() { }
	StructDef::~StructDef() { }
	TypeAlias::~TypeAlias() { }
	OpOverload::~OpOverload() { }
	StructBase::~StructBase() { }
	ArrayIndex::~ArrayIndex() { }
	ProtocolDef::~ProtocolDef() { }
	BracedBlock::~BracedBlock() { }
	ExtensionDef::~ExtensionDef() { }
	MemberAccess::~MemberAccess() { }
	DeferredExpr::~DeferredExpr() { }
	ArrayLiteral::~ArrayLiteral() { }
	NamespaceDecl::~NamespaceDecl() { }
	StringLiteral::~StringLiteral() { }
	PostfixUnaryOp::~PostfixUnaryOp() { }
	TupleDecompDecl::~TupleDecompDecl() { }
	ArrayDecompDecl::~ArrayDecompDecl() { }
	ForeignFuncDecl::~ForeignFuncDecl() { }
	ComputedProperty::~ComputedProperty() { }
	AssignOpOverload::~AssignOpOverload() { }
	SubscriptOpOverload::~SubscriptOpOverload() { }
	BreakableBracedBlock::~BreakableBracedBlock() { }


	Root::~Root()
	{
		// for(Expr* e : this->topLevelExpressions)
		// 	delete e;
	}
}

Codegen::CodegenInstance::~CodegenInstance()
{
}























