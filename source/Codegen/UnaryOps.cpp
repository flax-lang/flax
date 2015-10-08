// UnaryOpCodegen.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "../include/ast.h"
#include "../include/codegen.h"



using namespace Ast;
using namespace Codegen;

Result_t UnaryOp::codegen(CodegenInstance* cgi, llvm::Value* lhsPtr, llvm::Value* rhs)
{
	iceAssert(this->expr);
	Result_t res = this->expr->codegen(cgi);

	switch(this->op)
	{
		case ArithmeticOp::LogicalNot:
			return Result_t(cgi->builder.CreateICmpEQ(res.result.first, llvm::Constant::getNullValue(res.result.first->getType())), res.result.second);

		case ArithmeticOp::Minus:
			return Result_t(cgi->builder.CreateNeg(res.result.first), res.result.second);

		case ArithmeticOp::Plus:
			return res;

		case ArithmeticOp::Deref:
			if(!res.result.first->getType()->isPointerTy())
				error(this, "Cannot dereference non-pointer type!");

			return Result_t(cgi->builder.CreateLoad(res.result.first), res.result.first);

		case ArithmeticOp::AddrOf:
			if(!res.result.second)
				error(this, "Cannot take address of literal or whatever it is you're trying to take the address of!");

			return Result_t(res.result.second, 0);

		case ArithmeticOp::BitwiseNot:
			return Result_t(cgi->builder.CreateNot(res.result.first), res.result.second);

		default:
			error(this, "(%s:%d) -> Internal check failed: invalid unary operator", __FILE__, __LINE__);
	}
}







Result_t PostfixUnaryOp::codegen(CodegenInstance* cgi, llvm::Value* lhsPtr, llvm::Value* rhs)
{
	if(this->kind == Kind::ArrayIndex)
	{
		ArrayIndex* fake = new ArrayIndex(this->pin, this->expr, this->args.front());
		return fake->codegen(cgi, lhsPtr, rhs);
	}
	else
	{
		error(this, "enosup");
	}
}













