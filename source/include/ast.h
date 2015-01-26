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

namespace Codegen
{
	class CodegenInstance;
}

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


	extern uint32_t Attr_Invalid;
	extern uint32_t Attr_NoMangle;
	extern uint32_t Attr_VisPublic;
	extern uint32_t Attr_VisInternal;
	extern uint32_t Attr_VisPrivate;
	extern uint32_t Attr_ForceMangle;

	typedef std::pair<llvm::Value*, llvm::Value*> ValPtr_p;



	struct Expr
	{
		Expr(Parser::PosInfo pos) : posinfo(pos) { }
		virtual ~Expr() { }
		virtual ValPtr_p codegen(Codegen::CodegenInstance* cgi) = 0;

		uint32_t attribs;
		Parser::PosInfo posinfo;
		std::string type;
		VarType varType;
	};

	struct DummyExpr : Expr
	{
		DummyExpr(Parser::PosInfo pos) : Expr(pos) { }
		~DummyExpr() { }
		virtual ValPtr_p codegen(Codegen::CodegenInstance* cgi) override { return ValPtr_p(0, 0); }
	};

	struct VarArg : Expr
	{
		~VarArg() { }
		virtual ValPtr_p codegen(Codegen::CodegenInstance* cgi) override { return ValPtr_p(0, 0); }
	};


	struct Number : Expr
	{
		~Number() { }
		Number(Parser::PosInfo pos, double val) : Expr(pos), dval(val) { this->decimal = true; }
		Number(Parser::PosInfo pos, int64_t val) : Expr(pos), ival(val) { this->decimal = false; }
		virtual ValPtr_p codegen(Codegen::CodegenInstance* cgi) override;

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
		BoolVal(Parser::PosInfo pos, bool val) : Expr(pos), val(val) { }
		virtual ValPtr_p codegen(Codegen::CodegenInstance* cgi) override;

		bool val;
	};

	struct VarRef : Expr
	{
		~VarRef() { }
		VarRef(Parser::PosInfo pos, std::string name) : Expr(pos), name(name) { }
		virtual ValPtr_p codegen(Codegen::CodegenInstance* cgi) override;

		std::string name;
		Expr* initVal;
	};

	struct VarDecl : Expr
	{
		~VarDecl() { }
		VarDecl(Parser::PosInfo pos, std::string name, bool immut) : Expr(pos), name(name), immutable(immut) { }
		virtual ValPtr_p codegen(Codegen::CodegenInstance* cgi) override;

		std::string name;
		bool immutable;
		Expr* initVal;
	};

	struct BinOp : Expr
	{
		~BinOp() { }
		BinOp(Parser::PosInfo pos, Expr* lhs, ArithmeticOp operation, Expr* rhs) : Expr(pos), left(lhs), op(operation), right(rhs) { }
		virtual ValPtr_p codegen(Codegen::CodegenInstance* cgi) override;

		Expr* left;
		Expr* right;

		ArithmeticOp op;
		llvm::PHINode* phi;
	};

	struct FuncDecl : Expr
	{
		~FuncDecl() { }
		FuncDecl(Parser::PosInfo pos, std::string id, std::deque<VarDecl*> params, std::string ret) : Expr(pos), name(id), params(params)
		{ this->type = ret; }
		virtual ValPtr_p codegen(Codegen::CodegenInstance* cgi) override;

		bool hasVarArg;
		bool isFFI;
		std::string name;
		std::string mangledName;
		std::deque<VarDecl*> params;
	};

	struct Closure : Expr
	{
		Closure(Parser::PosInfo pos) : Expr(pos) { }
		~Closure() { }
		virtual ValPtr_p codegen(Codegen::CodegenInstance* cgi) override;

		std::deque<Expr*> statements;
	};

	struct Func : Expr
	{
		~Func() { }
		Func(Parser::PosInfo pos, FuncDecl* funcdecl, Closure* block) : Expr(pos), decl(funcdecl), closure(block) { }
		virtual ValPtr_p codegen(Codegen::CodegenInstance* cgi) override;

		FuncDecl* decl;
		Closure* closure;
	};

	struct FuncCall : Expr
	{
		~FuncCall() { }
		FuncCall(Parser::PosInfo pos, std::string target, std::deque<Expr*> args) : Expr(pos), name(target), params(args) { }
		virtual ValPtr_p codegen(Codegen::CodegenInstance* cgi) override;

		std::string name;
		std::deque<Expr*> params;
	};

	struct Return : Expr
	{
		~Return() { }
		Return(Parser::PosInfo pos, Expr* e) : Expr(pos), val(e) { }
		virtual ValPtr_p codegen(Codegen::CodegenInstance* cgi) override;

		Expr* val;
	};

	struct Import : Expr
	{
		~Import() { }
		Import(Parser::PosInfo pos, std::string name) : Expr(pos), module(name) { }
		virtual ValPtr_p codegen(Codegen::CodegenInstance* cgi) override { return ValPtr_p(nullptr, nullptr); }

		std::string module;
	};

	struct ForeignFuncDecl : Expr
	{
		~ForeignFuncDecl() { }
		ForeignFuncDecl(Parser::PosInfo pos, FuncDecl* func) : Expr(pos), decl(func) { }
		virtual ValPtr_p codegen(Codegen::CodegenInstance* cgi) override;

		FuncDecl* decl;
	};

	struct If : Expr
	{
		~If() { }
		If(Parser::PosInfo pos, std::deque<std::pair<Expr*, Closure*>> cases, Closure* ecase) : Expr(pos), cases(cases), final(ecase) { }
		virtual ValPtr_p codegen(Codegen::CodegenInstance* cgi) override;


		Closure* final;
		std::deque<std::pair<Expr*, Closure*>> cases;
	};

	struct WhileLoop : Expr
	{
		~WhileLoop() { }
		WhileLoop(Parser::PosInfo pos, Expr* _cond, Closure* _body, bool dowhile) : Expr(pos),
			cond(_cond), body(_body), isDoWhileVariant(dowhile) { }

		virtual ValPtr_p codegen(Codegen::CodegenInstance* cgi) override;

		Expr* cond;
		Closure* body;
		bool isDoWhileVariant;
	};

	struct UnaryOp : Expr
	{
		~UnaryOp() { }
		UnaryOp(Parser::PosInfo pos, ArithmeticOp op, Expr* expr) : Expr(pos), op(op), expr(expr) { }
		virtual ValPtr_p codegen(Codegen::CodegenInstance* cgi) override;

		ArithmeticOp op;
		Expr* expr;
	};

	// fuck
	struct Struct;
	struct OpOverload : Expr
	{
		~OpOverload() { }
		OpOverload(Parser::PosInfo pos, ArithmeticOp op) : Expr(pos), op(op) { }
		virtual ValPtr_p codegen(Codegen::CodegenInstance* cgi) override;

		Func* func;
		ArithmeticOp op;
		Struct* str;
	};

	struct Struct : Expr
	{
		~Struct() { }
		Struct(Parser::PosInfo pos, std::string name) : Expr(pos), name(name) { }
		virtual ValPtr_p codegen(Codegen::CodegenInstance* cgi) override;
		void createType(Codegen::CodegenInstance* cgi);

		bool didCreateType;
		Func* ifunc;
		llvm::Function* defifunc;
		llvm::Function* initFunc;

		std::deque<std::pair<Expr*, int>> typeList;
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
		MemberAccess(Parser::PosInfo pos, VarRef* tgt, Expr* mem) : Expr(pos), target(tgt), member(mem) { }
		virtual ValPtr_p codegen(Codegen::CodegenInstance* cgi) override;


		VarRef* target;
		Expr* member;
	};

	struct ArrayIndex : Expr
	{
		~ArrayIndex() { }
		ArrayIndex(Parser::PosInfo pos, VarRef* v, Expr* index) : Expr(pos), var(v), index(index) { }
		virtual ValPtr_p codegen(Codegen::CodegenInstance* cgi) override;

		VarRef* var;
		Expr* index;

		llvm::Value* cachedIndex;
	};

	struct StringLiteral : Expr
	{
		~StringLiteral() { }
		StringLiteral(Parser::PosInfo pos, std::string str) : Expr(pos), str(str) { }
		virtual ValPtr_p codegen(Codegen::CodegenInstance* cgi) override;

		std::string str;
	};

	struct CastedType : Expr
	{
		~CastedType() { }
		CastedType(Parser::PosInfo pos, std::string _name) : Expr(pos), name(_name) { }
		virtual ValPtr_p codegen(Codegen::CodegenInstance* cgi) override { return ValPtr_p(0, 0); };

		std::string name;
	};

	struct Root : Expr
	{
		Root() : Expr(Parser::PosInfo()) { }
		~Root() { }
		virtual ValPtr_p codegen(Codegen::CodegenInstance* cgi) override;

		// public functiondecls and type decls.
		std::deque<std::pair<FuncDecl*, llvm::Function*>> publicFuncs;
		std::deque<std::pair<Struct*, llvm::Type*>> publicTypes;

		// imported types. these exist, but we need to declare them manually while code-generating.
		std::deque<std::pair<FuncDecl*, llvm::Function*>> externalFuncs;
		std::deque<std::pair<Struct*, llvm::Type*>> externalTypes;

		// libraries referenced by 'import'
		std::deque<std::string> referencedLibraries;

		std::deque<Func*> functions;
		std::deque<Import*> imports;
		std::deque<Struct*> structs;
		std::deque<ForeignFuncDecl*> foreignfuncs;
	};
}









