// LiteralCodegen.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "../include/ast.h"
#include "../include/codegen.h"
#include "../include/llvm_all.h"

using namespace Ast;
using namespace Codegen;

ValPtr_p Number::codeGen()
{
	// check builtin type
	if(this->varType <= VarType::Uint64)
		return ValPtr_p(llvm::ConstantInt::get(getContext(), llvm::APInt(pow(2, (int) this->varType % 4) * 8, this->ival, this->varType > VarType::Int64)), 0);

	else if(this->type == "Float32" || this->type == "Float64")
		return ValPtr_p(llvm::ConstantFP::get(getContext(), llvm::APFloat(this->dval)), 0);

	error("(%s:%s:%d) -> Internal check failed: invalid number", __FILE__, __PRETTY_FUNCTION__, __LINE__);
	return ValPtr_p(0, 0);
}

ValPtr_p BoolVal::codeGen()
{
	return ValPtr_p(llvm::ConstantInt::get(getContext(), llvm::APInt(1, this->val, false)), 0);
}

ValPtr_p StringLiteral::codeGen()
{
	return ValPtr_p(mainBuilder.CreateGlobalStringPtr(this->str), 0);
}
