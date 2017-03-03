// Destructors.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "codegen.h"

using namespace Codegen;

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
	Typeid::~Typeid() { }
	Return::~Return() { }
	Import::~Import() { }
	IfStmt::~IfStmt() { }
	BoolVal::~BoolVal() { }
	NullVal::~NullVal() { }
	VarDecl::~VarDecl() { }
	UnaryOp::~UnaryOp() { }
	Dealloc::~Dealloc() { }
	EnumDef::~EnumDef() { }
	ForLoop::~ForLoop() { }
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
	ArraySlice::~ArraySlice() { }
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



	// lmao
	Result_t Expr::codegen(CodegenInstance* cgi, fir::Value* target)
	{
		return this->codegen(cgi, target ? target->getType() : 0, target);
	}

	Result_t Expr::codegen(Codegen::CodegenInstance* cgi)
	{
		return this->codegen(cgi, 0, 0);
	}

	Result_t Expr::codegen(Codegen::CodegenInstance* cgi, fir::Type* extratype)
	{
		return this->codegen(cgi, extratype, 0);
	}

}

Codegen::CodegenInstance::~CodegenInstance()
{
}























