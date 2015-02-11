// UnaryOpCodegen.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "../include/ast.h"
#include "../include/codegen.h"
#include "../include/llvm_all.h"

using namespace Ast;
using namespace Codegen;

Result_t UnaryOp::codegen(CodegenInstance* cgi)
{
	assert(this->expr);
	Result_t res = this->expr->codegen(cgi);

	switch(this->op)
	{
		case ArithmeticOp::LogicalNot:
			return Result_t(cgi->mainBuilder.CreateNot(res.result.first), 0);

		case ArithmeticOp::Minus:
			return Result_t(cgi->mainBuilder.CreateNeg(res.result.first), 0);

		case ArithmeticOp::Plus:
			return res;

		case ArithmeticOp::Deref:
			return Result_t(cgi->mainBuilder.CreateLoad(res.result.first), res.result.first);

		case ArithmeticOp::AddrOf:
			return Result_t(res.result.second, 0);

		default:
			error("this, (%s:%d) -> Internal check failed: invalid unary operator", __FILE__, __LINE__);
	}
}
