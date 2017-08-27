// ast.h
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once
#include "defs.h"
#include "precompile.h"

namespace pts
{
	struct Type;
}

namespace fir
{
	struct Type;
}

namespace sst
{
	struct Stmt;
	struct Expr;

	struct TypecheckState;
}

namespace ast
{
	struct Stmt : Locatable
	{
		Stmt(const Location& l) : Locatable(l) { }
		virtual ~Stmt();
		virtual sst::Stmt* typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) = 0;
	};

	struct Expr : Stmt
	{
		Expr(const Location& l) : Stmt(l) { }
		~Expr();

		virtual sst::Stmt* typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) = 0;
	};

	struct DeferredStmt : Stmt
	{
		DeferredStmt(const Location& l) : Stmt(l) { }
		~DeferredStmt();

		virtual sst::Stmt* typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override
		{
			return this->actual->typecheck(fs, inferred);
		}

		Stmt* actual = 0;
	};




	struct ImportStmt : Stmt
	{
		ImportStmt(const Location& l, std::string p) : Stmt(l), path(p) { }
		~ImportStmt();

		virtual sst::Stmt* typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;

		std::string path;
		std::string resolvedModule;
	};

	struct Block : Stmt
	{
		Block(const Location& l) : Stmt(l) { }
		~Block();

		virtual sst::Stmt* typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;

		std::vector<Stmt*> statements;
		std::vector<Stmt*> deferredStatements;
	};

	struct FuncDefn : Stmt
	{
		FuncDefn(const Location& l) : Stmt(l) { }
		~FuncDefn();

		virtual sst::Stmt* typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;

		struct Arg
		{
			std::string name;
			Location loc;
			pts::Type* type = 0;
		};

		std::string name;
		std::map<std::string, TypeConstraints_t> generics;

		std::vector<Arg> args;
		pts::Type* returnType = 0;

		Block* body = 0;

		PrivacyLevel privacy = PrivacyLevel::Internal;

		bool isEntry = false;
		bool noMangle = false;
	};

	struct ForeignFuncDefn : Stmt
	{
		ForeignFuncDefn(const Location& l) : Stmt(l) { }
		~ForeignFuncDefn();

		virtual sst::Stmt* typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;

		using Arg = FuncDefn::Arg;

		std::string name;

		std::vector<Arg> args;
		pts::Type* returnType = 0;

		bool isVarArg = false;
		PrivacyLevel privacy = PrivacyLevel::Internal;
	};

	struct VarDefn : Stmt
	{
		VarDefn(const Location& l, std::string name) : Stmt(l) { }
		~VarDefn();

		virtual sst::Stmt* typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;

		std::string name;
		pts::Type* type = 0;

		bool immut = false;
		Expr* initialiser = 0;

		PrivacyLevel privacy = PrivacyLevel::Internal;
		bool noMangle = false;
	};

	struct TupleDecompVarDefn : Stmt
	{
		TupleDecompVarDefn(const Location& l) : Stmt(l) { }
		~TupleDecompVarDefn();

		virtual sst::Stmt* typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;

		struct Mapping
		{
			Location loc;
			std::string name;
			bool ref = false;

			std::vector<Mapping> inner;
		};

		bool immut = false;
		Expr* initialiser = 0;
		std::vector<Mapping> mappings;
	};

	struct ArrayDecompVarDefn : Stmt
	{
		ArrayDecompVarDefn(const Location& l) : Stmt(l) { }
		~ArrayDecompVarDefn();

		virtual sst::Stmt* typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;

		bool immut = false;
		Expr* initialiser = 0;

		std::map<size_t, std::tuple<std::string, bool, Location>> mapping;
	};



	struct TypeExpr : Expr
	{
		TypeExpr(const Location& l, pts::Type* t) : Expr(l), type(t) { }
		~TypeExpr();

		virtual sst::Stmt* typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;

		pts::Type* type = 0;
	};

	struct Ident : Expr
	{
		Ident(const Location& l, std::string n) : Expr(l), name(n) { }
		~Ident();

		virtual sst::Stmt* typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;

		std::string name;
	};

	struct BinaryOp : Expr
	{
		BinaryOp(const Location& loc, Operator o, Expr* l, Expr* r) : Expr(loc), op(o), left(l), right(r) { }
		~BinaryOp();

		virtual sst::Stmt* typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;

		Operator op = Operator::Invalid;

		Expr* left = 0;
		Expr* right = 0;
	};

	struct UnaryOp : Expr
	{
		UnaryOp(const Location& l) : Expr(l) { }
		~UnaryOp();

		virtual sst::Stmt* typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;

		Operator op = Operator::Invalid;

		Expr* expr = 0;
	};

	struct FunctionCall : Expr
	{
		FunctionCall(const Location& l, std::string n) : Expr(l), name(n) { }
		~FunctionCall();

		virtual sst::Stmt* typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;

		std::string name;
		std::vector<Expr*> args;
	};

	struct DotOperator : Expr
	{
		DotOperator(const Location& loc, Expr* l, Expr* r) : Expr(loc), left(l), right(r) { }
		~DotOperator();

		virtual sst::Stmt* typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;

		Expr* left = 0;
		Expr* right = 0;
	};




	struct LitNumber : Expr
	{
		LitNumber(const Location& l, std::string n) : Expr(l), num(n) { }
		~LitNumber();

		virtual sst::Stmt* typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;

		std::string num;
	};

	struct LitBool : Expr
	{
		LitBool(const Location& l, bool val) : Expr(l), value(val) { }
		~LitBool();

		virtual sst::Stmt* typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;

		bool value = false;
	};

	struct LitString : Expr
	{
		LitString(const Location& l, std::string s, bool isc) : Expr(l), str(s), isCString(isc) { }
		~LitString();

		virtual sst::Stmt* typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;

		std::string str;
		bool isCString = false;
	};

	struct LitNull : Expr
	{
		LitNull(const Location& l) : Expr(l) { }
		~LitNull();

		virtual sst::Stmt* typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;
	};

	struct LitTuple : Expr
	{
		LitTuple(const Location& l, std::vector<Expr*> its) : Expr(l), values(its) { }
		~LitTuple();

		virtual sst::Stmt* typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;

		std::vector<Expr*> values;
	};



	struct TopLevelBlock : Expr
	{
		TopLevelBlock(const Location& l, std::string n) : Expr(l), name(n) { }
		~TopLevelBlock();

		virtual sst::Stmt* typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;

		std::string name;
		std::vector<Stmt*> statements;
		PrivacyLevel privacy = PrivacyLevel::Internal;
	};
}






