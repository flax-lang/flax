// Destructors.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "codegen.h"

namespace Ast
{
	DummyExpr::~DummyExpr()
	{

	}

	Number::~Number()
	{

	}

	BoolVal::~BoolVal()
	{

	}

	NullVal::~NullVal()
	{
	}

	ProtocolDef::~ProtocolDef()
	{
	}

	VarRef::~VarRef()
	{

	}

	VarDecl::~VarDecl()
	{
	}

	Tuple::~Tuple()
	{
	}

	BinOp::~BinOp()
	{

	}

	FuncDecl::~FuncDecl()
	{

	}

	BracedBlock::~BracedBlock()
	{
	}

	Func::~Func()
	{
	}

	FuncCall::~FuncCall()
	{
	}

	Typeof::~Typeof()
	{
	}

	Return::~Return()
	{
	}

	Import::~Import()
	{
	}

	ForeignFuncDecl::~ForeignFuncDecl()
	{
	}

	BreakableBracedBlock::~BreakableBracedBlock()
	{

	}

	IfStmt::~IfStmt()
	{
	}

	WhileLoop::~WhileLoop()
	{
	}

	ForLoop::~ForLoop()
	{

	}

	ComputedProperty::~ComputedProperty()
	{

	}

	Break::~Break()
	{

	}

	Continue::~Continue()
	{

	}

	UnaryOp::~UnaryOp()
	{
	}

	OpOverload::~OpOverload()
	{
	}

	SubscriptOpOverload::~SubscriptOpOverload()
	{
	}

	AssignOpOverload::~AssignOpOverload()
	{
	}

	StructBase::~StructBase()
	{
	}

	ExtensionDef::~ExtensionDef()
	{
	}

	StructDef::~StructDef()
	{
	}

	ClassDef::~ClassDef()
	{
	}

	MemberAccess::~MemberAccess()
	{
	}

	NamespaceDecl::~NamespaceDecl()
	{
	}

	ArrayIndex::~ArrayIndex()
	{

	}

	StringLiteral::~StringLiteral()
	{

	}

	Alloc::~Alloc()
	{
	}

	Dealloc::~Dealloc()
	{
	}

	EnumDef::~EnumDef()
	{
	}

	TypeAlias::~TypeAlias()
	{
	}

	DeferredExpr::~DeferredExpr()
	{
	}

	ArrayLiteral::~ArrayLiteral()
	{
	}

	PostfixUnaryOp::~PostfixUnaryOp()
	{
	}

	Root::~Root()
	{
		for(Expr* e : this->topLevelExpressions)
			delete e;
	}
}

Codegen::CodegenInstance::~CodegenInstance()
{
}























