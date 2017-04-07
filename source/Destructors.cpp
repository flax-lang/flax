// Destructors.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "cst.h"
#include "codegen.h"

using namespace Codegen;

namespace Ast
{
	Func::~Func() { }
	Tuple::~Tuple() { }
	BinOp::~BinOp() { }
	Break::~Break() { }
	Alloc::~Alloc() { }
	Range::~Range() { }
	Number::~Number() { }
	VarRef::~VarRef() { }
	Typeof::~Typeof() { }
	Typeid::~Typeid() { }
	Sizeof::~Sizeof() { }
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
	ForInLoop::~ForInLoop() { }
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
		return this->codegen(cgi, target ? (target->getType()->isPointerType()
			? target->getType()->getPointerElementType() : target->getType()) : 0, target);
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







namespace Cst
{
	DotOp::~DotOp() { }
	Block::~Block() { }
	StmtIf::~StmtIf() { }
	UnaryOp::~UnaryOp() { }
	SliceOp::~SliceOp() { }
	LitBool::~LitBool() { }
	LitNull::~LitNull() { }
	LoopFor::~LoopFor() { }
	BinaryOp::~BinaryOp() { }
	LitTuple::~LitTuple() { }
	LitArray::~LitArray() { }
	DefnEnum::~DefnEnum() { }
	LitInteger::~LitInteger() { }
	LitDecimal::~LitDecimal() { }
	LitString::~LitString() { }
	ExprRange::~ExprRange() { }
	DefnClass::~DefnClass() { }
	StmtAlloc::~StmtAlloc() { }
	LoopWhile::~LoopWhile() { }
	LoopForIn::~LoopForIn() { }
	ExprTypeof::~ExprTypeof() { }
	ExprTypeid::~ExprTypeid() { }
	ExprSizeof::~ExprSizeof() { }
	DefnStruct::~DefnStruct() { }
	SubscriptOp::~SubscriptOp() { }
	StmtDealloc::~StmtDealloc() { }
	ExprVariable::~ExprVariable() { }
	DeclVariable::~DeclVariable() { }
	DeclFunction::~DeclFunction() { }
	DefnProtocol::~DefnProtocol() { }
	DefnExtension::~DefnExtension() { }
	DeclTupleDecomp::~DeclTupleDecomp() { }
	DeclArrayDecomp::~DeclArrayDecomp() { }

	StmtBreak::~StmtBreak() { }
	StmtContinue::~StmtContinue() { }
	StmtReturn::~StmtReturn() { }

	ExprFuncCall::~ExprFuncCall() { }
}

















