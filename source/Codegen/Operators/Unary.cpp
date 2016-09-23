// Unary.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "codegen.h"
#include "operators.h"

using namespace Ast;
using namespace Codegen;



Result_t UnaryOp::codegen(CodegenInstance* cgi, fir::Value* extra)
{
	return Operators::OperatorMap::get().call(this->op, cgi, this, { this->expr });
}

fir::Type* UnaryOp::getType(CodegenInstance* cgi, bool allowFail, fir::Value* extra)
{
	if(this->op == ArithmeticOp::Deref)
	{
		fir::Type* ltype = this->expr->getType(cgi);
		if(!ltype->isPointerType())
			error(expr, "Attempted to dereference a non-pointer type '%s'", ltype->str().c_str());

		return ltype->getPointerElementType();
	}
	else if(this->op == ArithmeticOp::AddrOf)
	{
		fir::Type* t = this->expr->getType(cgi);
		if(cgi->isRefCountedType(t))
			error(this, "Reference counted types (such as '%s') cannot have their address taken", t->str().c_str());

		return t->getPointerTo();
	}
	else
	{
		return this->expr->getType(cgi);
	}
}




Result_t PostfixUnaryOp::codegen(CodegenInstance* cgi, fir::Value* extra)
{
	iceAssert(0);
}

fir::Type* PostfixUnaryOp::getType(CodegenInstance* cgi, bool allowFail, fir::Value* extra)
{
	iceAssert(0);
}







namespace Operators
{
	Result_t operatorUnaryPlus(CodegenInstance* cgi, ArithmeticOp op, Expr* user, std::deque<Expr*> args)
	{
		auto res = args[0]->codegen(cgi).result;

		// basically a no-op.
		return Result_t(res.first, res.second);
	}

	Result_t operatorUnaryMinus(CodegenInstance* cgi, ArithmeticOp op, Expr* user, std::deque<Expr*> args)
	{
		auto res = args[0]->codegen(cgi).result;

		return Result_t(cgi->irb.CreateNeg(res.first), res.second);
	}

	Result_t operatorBitwiseNot(CodegenInstance* cgi, ArithmeticOp op, Expr* user, std::deque<Expr*> args)
	{
		auto res = args[0]->codegen(cgi).result;
		if(!res.first->getType()->isIntegerType())
			error(user, "Cannot perform bitwise NOT (~) on a non-integer type (have type '%s')", res.first->getType()->str().c_str());

		return Result_t(cgi->irb.CreateBitwiseNOT(res.first), res.second);
	}

	Result_t operatorAddressOf(CodegenInstance* cgi, ArithmeticOp op, Expr* user, std::deque<Expr*> args)
	{
		auto res = args[0]->codegen(cgi).result;

		if(!res.second)
			error(user, "Cannot take the address of literal (have type '%s')", res.first->getType()->str().c_str());

		return Result_t(res.second, 0);
	}

	Result_t operatorDereference(CodegenInstance* cgi, ArithmeticOp op, Expr* user, std::deque<Expr*> args)
	{
		auto res = args[0]->codegen(cgi).result;

		if(!res.first->getType()->isPointerType())
			error(user, "Cannot dereference non-pointer type (have type '%s')", res.first->getType()->str().c_str());

		return Result_t(cgi->irb.CreateLoad(res.first), res.first);
	}
}
