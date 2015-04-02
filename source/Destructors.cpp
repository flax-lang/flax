// Destructors.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "include/ast.h"

namespace Ast
{
	DummyExpr::~DummyExpr()
	{

	}

	VarArg::~VarArg()
	{

	}

	Number::~Number()
	{

	}

	BoolVal::~BoolVal()
	{

	}

	VarRef::~VarRef()
	{

	}

	VarDecl::~VarDecl()
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

	If::~If()
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

	StructBase::~StructBase()
	{
	}

	Extension::~Extension()
	{
	}

	Struct::~Struct()
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

	Enumeration::~Enumeration()
	{
	}

	TypeAlias::~TypeAlias()
	{
	}

	DeferredExpr::~DeferredExpr()
	{
	}

	Root::~Root()
	{
		for(Expr* e : this->topLevelExpressions)
			delete e;
	}
}






