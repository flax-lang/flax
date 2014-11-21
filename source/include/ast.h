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

#include <map>
#include <string>
#include <deque>
#include "parser.h"

#include "llvm/IR/Value.h"
#include "llvm/IR/Instructions.h"

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

		Int8Ptr,
		Int16Ptr,
		Int32Ptr,
		Int64Ptr,

		Uint8Ptr,
		Uint16Ptr,
		Uint32Ptr,
		Uint64Ptr,

		// we do it this way so we can do math tricks on these to get the number of bits
		Bool,
		UserDefined,
		Float32,
		Float64,

		Void,
		AnyPtr,
		Array,
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

		LogicalNot,
		Plus,
		Minus,

		AddrOf,
		Deref,

		BitwiseAnd,
		BitwiseOr,
		BitwiseXor,

		LogicalAnd,
		LogicalOr,

		Cast,
	};

	typedef std::pair<llvm::Value*, llvm::Value*> ValPtr_p;



	struct Expr
	{
		virtual ~Expr() { }
		virtual ValPtr_p codeGen() = 0;

		Parser::PosInfo posinfo;
		std::string type;
		VarType varType;
	};

	struct DummyExpr : Expr
	{
		~DummyExpr() { }
		virtual ValPtr_p codeGen() override { return ValPtr_p(0, 0); }
	};


	struct Number : Expr
	{
		~Number() { }
		Number(double val) : dval(val) { this->decimal = true; }
		Number(int64_t val) : ival(val) { this->decimal = false; }
		virtual ValPtr_p codeGen() override;
		Number* setPos(Parser::PosInfo p) { this->posinfo = p; return this; }

		bool decimal = false;
		union
		{
			int64_t ival;
			double dval;
		};
	};

	struct BoolVal : Expr
	{
		~BoolVal() { }
		BoolVal(bool val) : val(val) { }
		virtual ValPtr_p codeGen() override;
		BoolVal* setPos(Parser::PosInfo p) { this->posinfo = p; return this; }

		bool val;
	};

	struct VarRef : Expr
	{
		~VarRef() { }
		VarRef(std::string name) : name(name) { }
		virtual ValPtr_p codeGen() override;
		VarRef* setPos(Parser::PosInfo p) { this->posinfo = p; return this; }

		std::string name;
		Expr* initVal;
	};

	struct VarDecl : Expr
	{
		~VarDecl() { }
		VarDecl(std::string name, bool immut) : name(name), immutable(immut) { }
		virtual ValPtr_p codeGen() override;
		VarDecl* setPos(Parser::PosInfo p) { this->posinfo = p; return this; }

		std::string name;
		bool immutable;
		Expr* initVal;
	};

	struct BinOp : Expr
	{
		~BinOp() { }
		BinOp(Expr* lhs, ArithmeticOp operation, Expr* rhs) : left(lhs), op(operation), right(rhs) { }
		virtual ValPtr_p codeGen() override;
		BinOp* setPos(Parser::PosInfo p) { this->posinfo = p; return this; }

		Expr* left;
		Expr* right;

		ArithmeticOp op;
		llvm::PHINode* phi;
	};

	struct FuncDecl : Expr
	{
		~FuncDecl() { }
		FuncDecl(std::string id, std::deque<VarDecl*> params, std::string ret) : name(id), params(params) { this->type = ret; }
		virtual ValPtr_p codeGen() override;
		FuncDecl* setPos(Parser::PosInfo p) { this->posinfo = p; return this; }

		bool isFFI;
		std::string name;
		std::deque<VarDecl*> params;
	};

	struct Closure : Expr
	{
		~Closure() { }
		virtual ValPtr_p codeGen() override;
		Closure* setPos(Parser::PosInfo p) { this->posinfo = p; return this; }

		std::deque<Expr*> statements;
	};

	struct Func : Expr
	{
		~Func() { }
		Func(FuncDecl* funcdecl, Closure* block) : decl(funcdecl), closure(block) { }
		virtual ValPtr_p codeGen() override;
		Func* setPos(Parser::PosInfo p) { this->posinfo = p; return this; }

		FuncDecl* decl;
		Closure* closure;
	};

	struct FuncCall : Expr
	{
		~FuncCall() { }
		FuncCall(std::string target, std::deque<Expr*> args) : name(target), params(args) { }
		virtual ValPtr_p codeGen() override;
		FuncCall* setPos(Parser::PosInfo p) { this->posinfo = p; return this; }

		std::string name;
		std::deque<Expr*> params;
	};

	struct Return : Expr
	{
		~Return() { }
		Return(Expr* e) : val(e) { }
		virtual ValPtr_p codeGen() override;
		Return* setPos(Parser::PosInfo p) { this->posinfo = p; return this; }

		Expr* val;
	};

	struct Import : Expr
	{
		~Import() { }
		Import(std::string name) : module(name) { }
		virtual ValPtr_p codeGen() override { return ValPtr_p(nullptr, nullptr); }
		Import* setPos(Parser::PosInfo p) { this->posinfo = p; return this; }

		std::string module;
	};

	struct ForeignFuncDecl : Expr
	{
		~ForeignFuncDecl() { }
		ForeignFuncDecl(FuncDecl* func) : decl(func) { }
		virtual ValPtr_p codeGen() override;
		ForeignFuncDecl* setPos(Parser::PosInfo p) { this->posinfo = p; return this; }

		FuncDecl* decl;
	};

	struct If : Expr
	{
		~If() { }
		If(std::deque<std::pair<Expr*, Closure*>> cases, Closure* ecase) : cases(cases), final(ecase) { }
		virtual ValPtr_p codeGen() override;
		If* setPos(Parser::PosInfo p) { this->posinfo = p; return this; }


		Closure* final;
		std::deque<std::pair<Expr*, Closure*>> cases;
	};

	struct UnaryOp : Expr
	{
		~UnaryOp() { }
		UnaryOp(ArithmeticOp op, Expr* expr) : op(op), expr(expr) { }
		virtual ValPtr_p codeGen() override;
		UnaryOp* setPos(Parser::PosInfo p) { this->posinfo = p; return this; }

		ArithmeticOp op;
		Expr* expr;
	};

	// fuck
	struct Struct;
	struct OpOverload : Expr
	{
		~OpOverload() { }
		OpOverload(ArithmeticOp op) : op(op) { }
		virtual ValPtr_p codeGen() override;
		OpOverload* setPos(Parser::PosInfo p) { this->posinfo = p; return this; }

		Func* func;
		ArithmeticOp op;
		Struct* str;
	};

	struct Struct : Expr
	{
		~Struct() { }
		Struct(std::string name) : name(name) { }
		virtual ValPtr_p codeGen() override;
		Struct* setPos(Parser::PosInfo p) { this->posinfo = p; return this; }
		void createType();

		bool didCreateType;
		Func* ifunc;
		llvm::Function* defifunc;
		llvm::Function* initFunc;

		std::map<std::string, int> nameMap;
		std::string name;
		std::deque<VarDecl*> members;
		std::deque<Func*> funcs;
		std::map<ArithmeticOp, OpOverload*> opmap;
		std::map<ArithmeticOp, llvm::Function*> lopmap;
	};

	struct MemberAccess : Expr
	{
		~MemberAccess() { }
		MemberAccess(VarRef* tgt, Expr* mem) : target(tgt), member(mem) { }
		virtual ValPtr_p codeGen() override;

		MemberAccess* setPos(Parser::PosInfo p) { this->posinfo = p; return this; }

		VarRef* target;
		Expr* member;
	};

	struct ArrayIndex : Expr
	{
		~ArrayIndex() { }
		ArrayIndex(VarRef* v, Expr* index) : var(v), index(index) { }
		virtual ValPtr_p codeGen() override;
		ArrayIndex* setPos(Parser::PosInfo p) { this->posinfo = p; return this; }

		VarRef* var;
		Expr* index;
	};

	struct StringLiteral : Expr
	{
		~StringLiteral() { }
		StringLiteral(std::string str) : str(str) { }
		virtual ValPtr_p codeGen() override;
		StringLiteral* setPos(Parser::PosInfo p) { this->posinfo = p; return this; }

		std::string str;
	};

	struct Root : Expr
	{
		~Root() { }
		virtual ValPtr_p codeGen() override;

		// todo: add stuff like imports, etc.
		std::deque<Func*> functions;
		std::deque<Import*> imports;
		std::deque<Struct*> structs;
		std::deque<ForeignFuncDecl*> foreignfuncs;
	};
}

namespace Codegen
{
	void doCodegen(Ast::Root* root);
}










