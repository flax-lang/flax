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
		if(this->initVal)
			delete this->initVal;
	}

	BinOp::~BinOp()
	{

	}

	FuncDecl::~FuncDecl()
	{

	}

	BracedBlock::~BracedBlock()
	{
		for(Expr* e : this->statements)
			delete e;
	}

	Func::~Func()
	{
		delete this->decl;
		delete this->block;
	}

	FuncCall::~FuncCall()
	{
		for(Expr* e : this->params)
			delete e;
	}

	Return::~Return()
	{
		if(this->val)
			delete this->val;
	}

	Import::~Import()
	{

	}

	ForeignFuncDecl::~ForeignFuncDecl()
	{
		delete this->decl;
	}

	BreakableBracedBlock::~BreakableBracedBlock()
	{

	}

	If::~If()
	{
		if(this->final)
			delete this->final;

		for(auto p : this->cases)
		{
			delete p.first;
			delete p.second;
		}
	}

	WhileLoop::~WhileLoop()
	{
		delete this->cond;
		delete this->body;
	}

	ForLoop::~ForLoop()
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
		delete this->expr;
	}

	OpOverload::~OpOverload()
	{
		delete this->func;

		// we don't own the struct
	}

	Struct::~Struct()
	{
		for(auto p : this->typeList)
			delete p.first;

		for(auto v : this->members)
			delete v;
	}

	MemberAccess::~MemberAccess()
	{
	}

	ScopeResolution::~ScopeResolution()
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

	CastedType::~CastedType()
	{

	}

	Alloc::~Alloc()
	{
		delete this->count;
	}

	Dealloc::~Dealloc()
	{
		delete this->var;
	}

	Root::~Root()
	{
		for(Expr* e : this->topLevelExpressions)
			delete e;
	}
}






