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
	Block::~Block() { }

	LitBool::~LitBool() { }
	LitNull::~LitNull() { }
	LitTuple::~LitTuple() { }
	LitArray::~LitArray() { }
	LitString::~LitString() { }
	LitInteger::~LitInteger() { }
	LitDecimal::~LitDecimal() { }


	LoopFor::~LoopFor() { }
	LoopWhile::~LoopWhile() { }
	LoopForIn::~LoopForIn() { }

	DeclVariable::~DeclVariable() { }
	DeclFunction::~DeclFunction() { }
	DeclTupleDecomp::~DeclTupleDecomp() { }
	DeclArrayDecomp::~DeclArrayDecomp() { }

	DefnType::~DefnType() { }
	DefnEnum::~DefnEnum() { }
	DefnClass::~DefnClass() { }
	DefnStruct::~DefnStruct() { }
	DefnProtocol::~DefnProtocol() { }
	DefnExtension::~DefnExtension() { }

	DotOp::~DotOp() { }
	UnaryOp::~UnaryOp() { }
	SliceOp::~SliceOp() { }
	BinaryOp::~BinaryOp() { }
	SubscriptOp::~SubscriptOp() { }

	ExprRange::~ExprRange() { }
	ExprTypeof::~ExprTypeof() { }
	ExprTypeid::~ExprTypeid() { }
	ExprSizeof::~ExprSizeof() { }
	ExprVariable::~ExprVariable() { }
	ExprFuncCall::~ExprFuncCall() { }

	StmtIf::~StmtIf() { }
	StmtBreak::~StmtBreak() { }
	StmtAlloc::~StmtAlloc() { }
	StmtReturn::~StmtReturn() { }
	StmtDealloc::~StmtDealloc() { }
	StmtContinue::~StmtContinue() { }
}

















