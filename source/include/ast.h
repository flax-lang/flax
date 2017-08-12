// ast.h
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once
#include "defs.h"

#include <map>
#include <vector>

namespace pts
{
	struct Type;
}

namespace ast
{
	struct Stmt
	{
		Stmt(const Location& l) : loc(l) { }
		virtual ~Stmt();

		Location loc;
	};

	struct Expr : Stmt
	{
		Expr(const Location& l) : Stmt(l) { }
		~Expr();
	};






	struct ImportStmt : Stmt
	{
		ImportStmt(const Location& l, std::string p) : Stmt(l), path(p) { }
		~ImportStmt();

		std::string path;
	};

	struct Block : Stmt
	{
		Block(const Location& l) : Stmt(l) { }
		~Block();

		std::vector<Stmt*> statements;
		std::vector<Stmt*> deferredStatements;
	};

	struct FuncDefn : Stmt
	{
		FuncDefn(const Location& l) : Stmt(l) { }
		~FuncDefn();

		struct Arg
		{
			std::string name;
			pts::Type* type = 0;
		};

		std::string name;
		std::map<std::string, TypeConstraints_t> generics;

		std::vector<Arg> args;
		pts::Type* returnType = 0;

		Block* body = 0;

		PrivacyLevel privacy = PrivacyLevel::Invalid;
	};

	struct ForeignFuncDefn : Stmt
	{

	};

	struct VarDefn : Stmt
	{
		VarDefn(const Location& l, std::string name) : Stmt(l) { }
		~VarDefn();

		std::string name;
		pts::Type* type = 0;

		bool immut = false;
		Expr* initialiser = 0;
	};

	struct TupleDecompVarDefn : Stmt
	{
		TupleDecompVarDefn(const Location& l) : Stmt(l) { }
		~TupleDecompVarDefn();

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

		bool immut = false;
		Expr* initialiser = 0;

		std::map<size_t, std::tuple<std::string, bool, Location>> mapping;
	};



	struct TypeExpr : Expr
	{
		TypeExpr(const Location& l, pts::Type* t) : Expr(l), type(t) { }
		~TypeExpr();

		pts::Type* type = 0;
	};

	struct Ident : Expr
	{
		Ident(const Location& l, std::string n) : Expr(l), name(n) { }
		~Ident();

		std::string name;
	};

	struct BinaryOp : Expr
	{
		BinaryOp(const Location& loc, Operator o, Expr* l, Expr* r) : Expr(loc), op(o), left(l), right(r) { }
		~BinaryOp();

		Operator op = Operator::Invalid;

		Expr* left = 0;
		Expr* right = 0;
	};

	struct UnaryOp : Expr
	{
		UnaryOp(const Location& l) : Expr(l) { }
		~UnaryOp();

		Operator op = Operator::Invalid;

		Expr* expr = 0;
	};

	struct FunctionCall : Expr
	{
	};

	struct DotOperator : Expr
	{
		DotOperator(const Location& loc, Expr* l, Expr* r) : Expr(loc), left(l), right(r) { }
		~DotOperator();

		Expr* left = 0;
		Expr* right = 0;
	};




	struct LitNumber : Expr
	{
		LitNumber(const Location& l, std::string n) : Expr(l), num(n) { }
		~LitNumber();

		std::string num;
	};

	struct LitBool : Expr
	{
		LitBool(const Location& l, bool val) : Expr(l), value(val) { }
		~LitBool();

		bool value = false;
	};

	struct LitString : Expr
	{
		LitString(const Location& l, std::string s) : Expr(l), str(s) { }
		~LitString();

		std::string str;
	};

	struct LitNull : Expr
	{
		LitNull(const Location& l) : Expr(l) { }
		~LitNull();
	};

	struct LitTuple : Expr
	{
		LitTuple(const Location& l, std::vector<Expr*> its) : Expr(l), values(its) { }
		~LitTuple();

		std::vector<Expr*> values;
	};



	struct TopLevelBlock : Expr
	{
		TopLevelBlock(const Location& l, std::string n) : Expr(l), name(n) { }
		~TopLevelBlock();

		std::string name;
		std::vector<Stmt*> statements;
	};
}






