// sst.h
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once
#include "defs.h"
#include "sst_expr.h"

namespace fir
{
	struct Type;
	struct FunctionType;
}

namespace cgn
{
	struct CodegenState;
}

namespace sst
{
	struct Defn : Stmt
	{
		Defn(const Location& l) : Stmt(l) { }
		~Defn() { }

		Identifier id;
		fir::Type* type = 0;
		bool global = false;
		PrivacyLevel privacy = PrivacyLevel::Internal;
	};


	struct TypeExpr : Expr
	{
		TypeExpr(const Location& l, fir::Type* t) : Expr(l, t) { }
		~TypeExpr() { }

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) override;
	};



	struct Block : Stmt
	{
		Block(const Location& l) : Stmt(l) { }
		~Block() { }

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) override;

		Location closingBrace;

		std::vector<std::string> scope;
		std::string generatedScopeName;

		std::vector<Stmt*> statements;
		std::vector<Stmt*> deferred;
	};

	struct IfStmt : Stmt
	{
		IfStmt(const Location& l) : Stmt(l) { }
		~IfStmt() { }

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) override;

		struct Case
		{
			Expr* cond = 0;
			Block* body = 0;

			std::vector<Stmt*> inits;
		};

		std::vector<std::string> scope;
		std::string generatedScopeName;
		std::vector<Case> cases;
		Block* elseCase = 0;
	};

	struct ReturnStmt : Stmt
	{
		ReturnStmt(const Location& l) : Stmt(l) { }
		~ReturnStmt() { }

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) override;

		Expr* value = 0;
		fir::Type* expectedType = 0;
	};











	struct BinaryOp : Expr
	{
		BinaryOp(const Location& l, fir::Type* t) : Expr(l, t) { }
		~BinaryOp() { }

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) override;

		Expr* left = 0;
		Expr* right = 0;
		Operator op = Operator::Invalid;
	};

	struct UnaryOp : Expr
	{
		UnaryOp(const Location& l, fir::Type* t) : Expr(l, t) { }
		~UnaryOp() { }

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) override;

		Expr* expr = 0;
		Operator op = Operator::Invalid;
	};

	struct AssignOp : Expr
	{
		AssignOp(const Location& l);
		~AssignOp() { }

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) override;

		Operator op = Operator::Invalid;

		Expr* left = 0;
		Expr* right = 0;
	};


	struct SubscriptOp : Expr
	{
		SubscriptOp(const Location& l, fir::Type* t) : Expr(l, t) { }
		~SubscriptOp() { }

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) override;

		Expr* expr = 0;
		Expr* inside = 0;

		// to assist safety checks, store the generated things
		fir::Value* cgSubscriptee = 0;
		fir::Value* cgIndex = 0;
	};


	struct FunctionDecl;
	struct FunctionCall : Expr
	{
		FunctionCall(const Location& l, fir::Type* t) : Expr(l, t) { }
		~FunctionCall() { }

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) override;

		std::string name;
		Defn* target = 0;
		std::vector<Expr*> arguments;
	};

	struct VarDefn;
	struct VarRef : Expr
	{
		VarRef(const Location& l, fir::Type* t) : Expr(l, t) { }
		~VarRef() { }

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) override;

		std::string name;
		Defn* def = 0;
	};



	struct LiteralNumber : Expr
	{
		LiteralNumber(const Location& l, fir::Type* t) : Expr(l, t) { }
		~LiteralNumber() { }

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) override;

		mpfr::mpreal number;
	};

	struct LiteralString : Expr
	{
		LiteralString(const Location& l, fir::Type* t) : Expr(l, t) { }
		~LiteralString() { }

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) override;

		std::string str;
		bool isCString = false;
	};

	struct LiteralNull : Expr
	{
		LiteralNull(const Location& l, fir::Type* t) : Expr(l, t) { }
		~LiteralNull() { }

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) override;
	};

	struct LiteralBool : Expr
	{
		LiteralBool(const Location& l, fir::Type* t) : Expr(l, t) { }
		~LiteralBool() { }

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) override;

		bool value = false;
	};

	struct LiteralTuple : Expr
	{
		LiteralTuple(const Location& l, fir::Type* t) : Expr(l, t) { }
		~LiteralTuple() { }

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) override;

		std::vector<Expr*> values;
	};

	struct LiteralArray : Expr
	{
		LiteralArray(const Location& l, fir::Type* t) : Expr(l, t) { }
		~LiteralArray() { }

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) override;

		std::vector<Expr*> values;
	};



	struct NamespaceDefn : Stmt
	{
		NamespaceDefn(const Location& l) : Stmt(l) { }
		~NamespaceDefn() { }

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) override;

		std::string name;
		std::vector<Stmt*> statements;
	};

	struct ArgumentDefn : Defn
	{
		ArgumentDefn(const Location& l) : Defn(l) { }
		~ArgumentDefn() { }

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) override;
	};

	struct VarDefn : Defn
	{
		VarDefn(const Location& l) : Defn(l) { }
		~VarDefn() { }

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) override;

		Expr* init = 0;
		bool immutable = false;
	};

	struct FunctionDecl : Defn
	{
		struct Param
		{
			std::string name;
			Location loc;
			fir::Type* type = 0;
		};

		std::vector<Param> params;
		fir::Type* returnType = 0;

		bool isEntry = false;
		bool noMangle = false;
		bool isVarArg = false;

		protected:
		FunctionDecl(const Location& l) : Defn(l) { }
		~FunctionDecl() { }
	};

	struct FunctionDefn : FunctionDecl
	{
		FunctionDefn(const Location& l) : FunctionDecl(l) { }
		~FunctionDefn() { }

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) override;

		std::vector<ArgumentDefn*> arguments;

		Block* body = 0;
		bool needReturnVoid = false;
	};

	struct ForeignFuncDefn : FunctionDecl
	{
		ForeignFuncDefn(const Location& l) : FunctionDecl(l) { }
		~ForeignFuncDefn() { }

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) override;
	};

	struct TupleDecompDefn : Stmt
	{
		TupleDecompDefn(const Location& l) : Stmt(l) { }
		~TupleDecompDefn() { }

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) override;
	};

	struct ArrayDecompDefn : Stmt
	{
		ArrayDecompDefn(const Location& l) : Stmt(l) { }
		~ArrayDecompDefn() { }

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) override;
	};
}

















