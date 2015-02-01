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
	switch(this->op)
	{
		case ArithmeticOp::LogicalNot:
			return Result_t(cgi->mainBuilder.CreateNot(this->expr->codegen(cgi).result.first), 0);

		case ArithmeticOp::Minus:
			return Result_t(cgi->mainBuilder.CreateNeg(this->expr->codegen(cgi).result.first), 0);

		case ArithmeticOp::Plus:
			return this->expr->codegen(cgi);

		case ArithmeticOp::Deref:
		{
			Result_t vp = this->expr->codegen(cgi);
			return Result_t(cgi->mainBuilder.CreateLoad(vp.result.first), vp.result.first);
		}

		case ArithmeticOp::AddrOf:
		{
			return Result_t(this->expr->codegen(cgi).result.second, 0);
		}

		default:
			error("this, (%s:%d) -> Internal check failed: invalid unary operator", __FILE__, __LINE__);
			return Result_t(0, 0);
	}
}
