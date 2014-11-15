// ast.h
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once

#ifndef __STDC_CONSTANT_MACROS
#define __STDC_CONSTANT_MACROS
#endif

#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS
#endif


#include <string>
#include <deque>
#include "parser.h"

#include "llvm/IR/Value.h"

namespace Ast
{
	// rant:
	// fuck this. c++ structs are exactly the same as classes, except with public visibility by default
	// i'm lazy so this is the way it'll be.

	enum class VarType
	{
		Int8,
		Int16,
		Int32,
		Int64,

		Uint8,
		Uint16,
		Uint32,
		Uint64,

		// we do it this way so we can do math tricks on these to get the number of bits
		Bool,
		UserDefined,
		Float32,
		Float64,

		Void,
	};

	enum class ArithmeticOp
	{
		Add,
		Subtract,
		Multiply,
		Divide,
		Modulo,
		ShiftLeft,
		ShiftRight,
		Assign,

		CmpLT,
		CmpGT,
		CmpLEq,
		CmpGEq,
		CmpEq,
		CmpNEq,
	};

	struct Expr
	{
		virtual ~Expr() { }
		virtual void print() { }
		virtual llvm::Value* codeGen() = 0;
		std::string type;
		VarType varType;
	};

	struct Number : Expr
	{
		~Number() { }
		Number(double val) : dval(val) { this->decimal = true; }
		Number(int64_t val) : ival(val) { this->decimal = false; }
		virtual void print() override;
		virtual llvm::Value* codeGen() override;

		bool decimal = false;
		union
		{
			int64_t ival;
			double dval;
		};
	};

	struct VarRef : Expr
	{
		~VarRef() { }
		VarRef(std::string& name) : name(name) { }
		virtual void print() override;
		virtual llvm::Value* codeGen() override;

		std::string name;
		Expr* initVal;
	};

	struct VarDecl : Expr
	{
		~VarDecl() { }
		VarDecl(std::string& name, bool immut) : name(name), immutable(immut) { }
		virtual void print() override;
		virtual llvm::Value* codeGen() override;

		std::string name;
		bool immutable;
		Expr* initVal;
	};

	struct BinOp : Expr
	{
		~BinOp() { }
		BinOp(Expr* lhs, ArithmeticOp operation, Expr* rhs) : left(lhs), op(operation), right(rhs) { }
		virtual void print() override;
		virtual llvm::Value* codeGen() override;

		Expr* left;
		Expr* right;

		ArithmeticOp op;
	};

	struct FuncDecl : Expr
	{
		~FuncDecl() { }
		FuncDecl(std::string id, std::deque<VarDecl*> params, std::string ret) : name(id), params(params)
		{
			this->type = ret;
		}
		virtual void print() override;
		virtual llvm::Value* codeGen() override;

		std::string name;
		std::deque<VarDecl*> params;
	};

	struct Func : Expr
	{
		~Func() { }
		Func(FuncDecl* funcdecl) : decl(funcdecl) { }
		virtual void print() override;
		virtual llvm::Value* codeGen() override;

		FuncDecl* decl;
		std::deque<Expr*> statements;
	};

	struct FuncCall : Expr
	{
		~FuncCall() { }
		FuncCall(std::string target, std::deque<Expr*> args) : name(target), params(args) { }
		virtual void print() override;
		virtual llvm::Value* codeGen() override;

		std::string name;
		std::deque<Expr*> params;
	};

	struct Return : Expr
	{
		~Return() { }
		Return(Expr* e) : val(e) { }
		virtual void print() override;
		virtual llvm::Value* codeGen() override;

		Expr* val;
	};

	struct Import : Expr
	{
		~Import() { }
		Import(std::string name) : module(name) { }
		virtual void print() override;
		virtual llvm::Value* codeGen() override { return nullptr; }

		std::string module;
	};

	struct ForeignFuncDecl : Expr
	{
		~ForeignFuncDecl() { }
		ForeignFuncDecl(FuncDecl* func) : decl(func) { }
		virtual llvm::Value* codeGen() override;

		FuncDecl* decl;
	};

	struct Root : Expr
	{
		~Root() { }
		virtual void print() override;
		virtual llvm::Value* codeGen() override;

		// todo: add stuff like imports, etc.
		std::deque<Func*> functions;
		std::deque<Import*> imports;
		std::deque<ForeignFuncDecl*> foreignfuncs;
	};
}

namespace Codegen
{
	void doCodegen(Ast::Root* root);
}










