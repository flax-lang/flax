// operators.h
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include <stdint.h>

#include "ast.h"

namespace Codegen
{
	struct CodegenInstance;
}
namespace fir
{
	struct Value;
}

namespace Operators
{
	struct OperatorMap
	{
		std::map<Ast::ArithmeticOp, std::function<Ast::Result_t(Codegen::CodegenInstance*, Ast::ArithmeticOp,
			Ast::Expr*, std::deque<Ast::Expr*>)>> theMap;

		OperatorMap();
		Ast::Result_t call(Ast::ArithmeticOp op, Codegen::CodegenInstance* cgi, Ast::Expr* usr, std::deque<Ast::Expr*> args);

		static OperatorMap& get();
	};


	Ast::Result_t operatorCustom(Codegen::CodegenInstance* cgi, Ast::ArithmeticOp op, Ast::Expr* usr, std::deque<Ast::Expr*> args);

	Ast::Result_t operatorCast(Codegen::CodegenInstance* cgi, Ast::ArithmeticOp op, Ast::Expr* usr, std::deque<Ast::Expr*> args);

	Ast::Result_t generalArithmeticOperator(Codegen::CodegenInstance* cgi, Ast::ArithmeticOp op, Ast::Expr* usr, std::deque<Ast::Expr*> args);

	Ast::Result_t operatorLogicalNot(Codegen::CodegenInstance* cgi, Ast::ArithmeticOp op, Ast::Expr* usr, std::deque<Ast::Expr*> args);
	Ast::Result_t operatorLogicalAnd(Codegen::CodegenInstance* cgi, Ast::ArithmeticOp op, Ast::Expr* usr, std::deque<Ast::Expr*> args);
	Ast::Result_t operatorLogicalOr(Codegen::CodegenInstance* cgi, Ast::ArithmeticOp op, Ast::Expr* usr, std::deque<Ast::Expr*> args);

	Ast::Result_t operatorBitwiseNot(Codegen::CodegenInstance* cgi, Ast::ArithmeticOp op, Ast::Expr* usr, std::deque<Ast::Expr*> args);
	Ast::Result_t operatorUnaryPlus(Codegen::CodegenInstance* cgi, Ast::ArithmeticOp op, Ast::Expr* usr, std::deque<Ast::Expr*> args);
	Ast::Result_t operatorUnaryMinus(Codegen::CodegenInstance* cgi, Ast::ArithmeticOp op, Ast::Expr* usr, std::deque<Ast::Expr*> args);
	Ast::Result_t operatorAddressOf(Codegen::CodegenInstance* cgi, Ast::ArithmeticOp op, Ast::Expr* usr, std::deque<Ast::Expr*> args);
	Ast::Result_t operatorDereference(Codegen::CodegenInstance* cgi, Ast::ArithmeticOp op, Ast::Expr* usr, std::deque<Ast::Expr*> args);

	Ast::Result_t operatorAssign(Codegen::CodegenInstance* cgi, Ast::ArithmeticOp op, Ast::Expr* usr, std::deque<Ast::Expr*> args);
	Ast::Result_t generalCompoundAssignOperator(Codegen::CodegenInstance* cgi, Ast::ArithmeticOp op, Ast::Expr* usr, std::deque<Ast::Expr*> args);

	Ast::Result_t performActualAssignment(Codegen::CodegenInstance* cgi, Ast::Expr* usr, Ast::Expr* leftExpr, Ast::Expr* rightExpr, Ast::ArithmeticOp op, fir::Value* lhs, fir::Value* lhsPtr, fir::Value* rhs, fir::Value* rhsPtr);

	Ast::Result_t operatorSubscript(Codegen::CodegenInstance* cgi, Ast::ArithmeticOp op, Ast::Expr* usr, std::deque<Ast::Expr*> args);
	Ast::Result_t operatorOverloadedSubscript(Codegen::CodegenInstance* cgi, Ast::ArithmeticOp op, Ast::Expr* usr, std::deque<Ast::Expr*> args);
	Ast::Result_t operatorAssignToOverloadedSubscript(Codegen::CodegenInstance* cgi, Ast::ArithmeticOp op, Ast::Expr* usr, Ast::Expr* lhs, fir::Value* rhs);
}













