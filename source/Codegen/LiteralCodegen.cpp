// LiteralCodegen.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "../include/ast.h"
#include "../include/codegen.h"
#include "../include/llvm_all.h"

using namespace Ast;
using namespace Codegen;

Result_t Number::codegen(CodegenInstance* cgi)
{
	// check builtin type
	if(this->varType <= VarType::Uint64)
		return Result_t(llvm::ConstantInt::get(cgi->getContext(), llvm::APInt(pow(2, (int) this->varType % 4) * 8, this->ival, this->varType > VarType::Int64)), 0);

	else if(this->type == "Float32" || this->type == "Float64")
		return Result_t(llvm::ConstantFP::get(cgi->getContext(), llvm::APFloat(this->dval)), 0);

	error("(%s:%s:%d) -> Internal check failed: invalid number", __FILE__, __PRETTY_FUNCTION__, __LINE__);
	return Result_t(0, 0);
}

Result_t BoolVal::codegen(CodegenInstance* cgi)
{
	return Result_t(llvm::ConstantInt::get(cgi->getContext(), llvm::APInt(1, this->val, false)), 0);
}

Result_t StringLiteral::codegen(CodegenInstance* cgi)
{
	return Result_t(cgi->mainBuilder.CreateGlobalStringPtr(this->str), 0);
}
