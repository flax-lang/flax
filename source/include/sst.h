// sst.h
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once
#include "defs.h"

namespace fir
{
	struct Type;
}

namespace sst
{
	struct Stmt
	{
		Stmt(const Location& l) : loc(l) { }
		virtual ~Stmt() { }

		Location loc;
	};

	struct Expr : Stmt
	{
		Expr(const Location& l) : Stmt(l) { }
		~Expr() { }

		fir::Type* type = 0;
	};






	struct Block : Expr
	{
		Block(const Location& l) : Expr(l) { }
		~Block() { }

		std::vector<Stmt*> statements;
		std::vector<Stmt*> deferred;
	};

	struct BinaryOp : Expr
	{
		BinaryOp(const Location& l) : Expr(l) { }
		~BinaryOp() { }
	};

	struct UnaryOp : Expr
	{
		UnaryOp(const Location& l) : Expr(l) { }
		~UnaryOp() { }
	};

	struct FunctionCall : Expr
	{
		FunctionCall(const Location& l) : Expr(l) { }
		~FunctionCall() { }
	};

	struct VarRef : Expr
	{
		VarRef(const Location& l) : Expr(l) { }
		~VarRef() { }
	};



	struct LiteralInt : Expr
	{
		LiteralInt(const Location& l) : Expr(l) { }
		~LiteralInt() { }

		size_t number = 0;
	};

	struct LiteralDec : Expr
	{
		LiteralDec(const Location& l) : Expr(l) { }
		~LiteralDec() { }

		long double number = 0.0;
	};

	struct LiteralString : Expr
	{
		LiteralString(const Location& l) : Expr(l) { }
		~LiteralString() { }

		std::string str;
	};

	struct LiteralNull : Expr
	{
		LiteralNull(const Location& l) : Expr(l) { }
		~LiteralNull() { }
	};

	struct LiteralBool : Expr
	{
		LiteralBool(const Location& l) : Expr(l) { }
		~LiteralBool() { }

		bool value = false;
	};

	struct LiteralTuple : Expr
	{
		LiteralTuple(const Location& l) : Expr(l) { }
		~LiteralTuple() { }

		std::vector<Expr*> values;
	};



	struct NamespaceDefn : Stmt
	{
		NamespaceDefn(const Location& l) : Stmt(l) { }
		~NamespaceDefn() { }

		std::vector<Stmt*> statements;
	};

	struct VarDefn : Stmt
	{
		VarDefn(const Location& l) : Stmt(l) { }
		~VarDefn() { }
	};

	struct FunctionDefn : Stmt
	{
		FunctionDefn(const Location& l) : Stmt(l) { }
		~FunctionDefn() { }

		struct Param
		{
			std::string name;
			fir::Type* type = 0;
		};
	};

	struct ForeignFuncDefn : Stmt
	{
		ForeignFuncDefn(const Location& l) : Stmt(l) { }
		~ForeignFuncDefn() { }

		using Param = FunctionDefn::Param;


		std::string name;
		std::vector<Param> params;

		fir::Type* returnType = 0;

		bool isVarArg = false;
		PrivacyLevel privacy = PrivacyLevel::Internal;
	};

	struct TupleDecompDefn : Stmt
	{
		TupleDecompDefn(const Location& l) : Stmt(l) { }
		~TupleDecompDefn() { }
	};

	struct ArrayDecompDefn : Stmt
	{
		ArrayDecompDefn(const Location& l) : Stmt(l) { }
		~ArrayDecompDefn() { }
	};
}

















