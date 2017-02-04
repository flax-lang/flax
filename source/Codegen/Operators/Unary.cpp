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
	Result_t operatorUnaryPlus(CodegenInstance* cgi, ArithmeticOp op, Expr* user, std::vector<Expr*> args)
	{
		auto res = args[0]->codegen(cgi);

		// basically a no-op.
		return Result_t(res.value, res.pointer);
	}

	Result_t operatorUnaryMinus(CodegenInstance* cgi, ArithmeticOp op, Expr* user, std::vector<Expr*> args)
	{
		auto res = args[0]->codegen(cgi);

		return Result_t(cgi->irb.CreateNeg(res.value), res.pointer);
	}

	Result_t operatorBitwiseNot(CodegenInstance* cgi, ArithmeticOp op, Expr* user, std::vector<Expr*> args)
	{
		auto res = args[0]->codegen(cgi);
		if(!res.value->getType()->isIntegerType())
			error(user, "Cannot perform bitwise NOT (~) on a non-integer type (have type '%s')", res.value->getType()->str().c_str());

		return Result_t(cgi->irb.CreateBitwiseNOT(res.value), res.pointer);
	}

	Result_t operatorAddressOf(CodegenInstance* cgi, ArithmeticOp op, Expr* user, std::vector<Expr*> args)
	{
		auto res = args[0]->codegen(cgi);

		if(res.valueKind != ValueKind::LValue)
			error(user, "Cannot take the address of an rvalue");

		iceAssert(res.pointer);
		return Result_t(res.pointer, 0);
	}

	Result_t operatorDereference(CodegenInstance* cgi, ArithmeticOp op, Expr* user, std::vector<Expr*> args)
	{
		auto res = args[0]->codegen(cgi);

		if(!res.value->getType()->isPointerType())
			error(user, "Cannot dereference non-pointer type (have type '%s')", res.value->getType()->str().c_str());

		if(res.pointer && res.pointer->isImmutable())
			res.value->makeImmutable();

		return Result_t(cgi->irb.CreateLoad(res.value), res.value, ValueKind::LValue);
	}
}











