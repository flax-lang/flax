// UnaryOpCodegen.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "codegen.h"



using namespace Ast;
using namespace Codegen;

Result_t UnaryOp::codegen(CodegenInstance* cgi, fir::Value* lhsPtr, fir::Value* rhs)
{
	iceAssert(this->expr);
	Result_t res = this->expr->codegen(cgi);

	switch(this->op)
	{
		case ArithmeticOp::LogicalNot:
			return Result_t(cgi->builder.CreateLogicalNot(res.result.first), res.result.second);

		case ArithmeticOp::Minus:
			return Result_t(cgi->builder.CreateNeg(res.result.first), res.result.second);

		case ArithmeticOp::Plus:
			return res;

		case ArithmeticOp::Deref:
			if(!res.result.first->getType()->isPointerType())
				error(this, "Cannot dereference non-pointer type");

			return Result_t(cgi->builder.CreateLoad(res.result.first), res.result.first);

		case ArithmeticOp::AddrOf:
			if(!res.result.second)
				error(this, "Cannot take the address of literal");

			return Result_t(res.result.second, 0);

		case ArithmeticOp::BitwiseNot:
			return Result_t(cgi->builder.CreateBitwiseNOT(res.result.first), res.result.second);

		default:
			error(this, "(%s:%d) -> Internal check failed: invalid unary operator", __FILE__, __LINE__);
	}
}







Result_t PostfixUnaryOp::codegen(CodegenInstance* cgi, fir::Value* lhsPtr, fir::Value* rhs)
{
	if(this->kind == Kind::ArrayIndex)
	{
		ArrayIndex* fake = new ArrayIndex(this->pin, this->expr, this->args.front());
		return fake->codegen(cgi, lhsPtr, rhs);
	}
	else
	{
		error(this, "enotsup");
	}
}













