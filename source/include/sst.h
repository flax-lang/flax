// sst.h
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once
#include "defs.h"

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
	struct Stmt : Locatable
	{
		Stmt(const Location& l) : Locatable(l) { }
		virtual ~Stmt() { }

		virtual CGResult codegen(cgn::CodegenState* cs, fir::Type* inferred = 0)
		{
			if(didCodegen)
			{
				return cachedResult;
			}
			else
			{
				this->didCodegen = true;
				return (this->cachedResult = this->_codegen(cs, inferred));
			}
		}

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) = 0;

		bool didCodegen = false;
		CGResult cachedResult = CGResult(0);
	};

	struct Expr : Stmt
	{
		Expr(const Location& l) : Stmt(l) { }
		~Expr() { }

		fir::Type* type = 0;
	};

	struct Defn : Stmt
	{
		Defn(const Location& l) : Stmt(l) { }
		~Defn() { }

		Identifier id;
		fir::Type* type = 0;
		bool global = false;
		PrivacyLevel privacy = PrivacyLevel::Internal;
	};




	struct Block : Expr
	{
		Block(const Location& l) : Expr(l) { }
		~Block() { }

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) override;

		std::vector<Stmt*> statements;
		std::vector<Stmt*> deferred;
	};

	struct BinaryOp : Expr
	{
		BinaryOp(const Location& l) : Expr(l) { }
		~BinaryOp() { }

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) override;

		Expr* left = 0;
		Expr* right = 0;
		Operator op = Operator::Invalid;
	};

	struct UnaryOp : Expr
	{
		UnaryOp(const Location& l) : Expr(l) { }
		~UnaryOp() { }

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) override;
	};

	struct FunctionDecl;
	struct FunctionCall : Expr
	{
		FunctionCall(const Location& l) : Expr(l) { }
		~FunctionCall() { }

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) override;

		std::string name;
		FunctionDecl* target = 0;
		std::vector<Expr*> arguments;
	};

	struct VarDefn;
	struct VarRef : Expr
	{
		VarRef(const Location& l) : Expr(l) { }
		~VarRef() { }

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) override;

		std::string name;
		Defn* def = 0;
	};



	struct LiteralInt : Expr
	{
		LiteralInt(const Location& l) : Expr(l) { }
		~LiteralInt() { }

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) override;

		size_t number = 0;
		bool negative = false;
	};

	struct LiteralDec : Expr
	{
		LiteralDec(const Location& l) : Expr(l) { }
		~LiteralDec() { }

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) override;

		long double number = 0.0;
	};

	struct LiteralString : Expr
	{
		LiteralString(const Location& l) : Expr(l) { }
		~LiteralString() { }

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) override;

		std::string str;
		bool isCString = false;
	};

	struct LiteralNull : Expr
	{
		LiteralNull(const Location& l) : Expr(l) { }
		~LiteralNull() { }

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) override;
	};

	struct LiteralBool : Expr
	{
		LiteralBool(const Location& l) : Expr(l) { }
		~LiteralBool() { }

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) override;

		bool value = false;
	};

	struct LiteralTuple : Expr
	{
		LiteralTuple(const Location& l) : Expr(l) { }
		~LiteralTuple() { }

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

		Block* body = 0;
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

















