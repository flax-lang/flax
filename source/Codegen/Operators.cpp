// Operators.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "ast.h"
#include "codegen.h"
#include "operators.h"

using namespace Ast;
using namespace Codegen;


Result_t ArrayIndex::codegen(CodegenInstance* cgi, fir::Value* extra)
{
	return Operators::OperatorMap::get().call(ArithmeticOp::Subscript, cgi, this, { this->arr, this->index });
}


Result_t BinOp::codegen(CodegenInstance* cgi, fir::Value* extra)
{
	iceAssert(this->left && this->right);
	return Operators::OperatorMap::get().call(this->op, cgi, this, { this->left, this->right });
}


Result_t UnaryOp::codegen(CodegenInstance* cgi, fir::Value* extra)
{
	return Operators::OperatorMap::get().call(this->op, cgi, this, { this->expr });
}

Result_t PostfixUnaryOp::codegen(CodegenInstance* cgi, fir::Value* extra)
{
	if(this->kind == Kind::ArrayIndex)
	{
		ArrayIndex* fake = new ArrayIndex(this->pin, this->expr, this->args.front());
		return fake->codegen(cgi, extra);
	}
	else
	{
		error(this, "enotsup");
	}
}





































