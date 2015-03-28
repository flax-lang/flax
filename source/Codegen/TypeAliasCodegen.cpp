// TypeAliasCodegen.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "../include/ast.h"
#include "../include/codegen.h"
#include "../include/llvm_all.h"

using namespace Ast;
using namespace Codegen;

Result_t TypeAlias::codegen(CodegenInstance* cgi, llvm::Value* lhsPtr, llvm::Value* rhs)
{
	return Result_t(0, 0);
}

void TypeAlias::createType(CodegenInstance* cgi)
{
	llvm::Type* targetType = 0;

	if(!this->isStrong)
	{
		targetType = cgi->unwrapPointerType(this, this->origType);
	}
	else
	{
		targetType = llvm::StructType::create(this->name, cgi->unwrapPointerType(this, this->origType), NULL);
		warn(this, "Strong type aliases are still iffy, use at your own risk");
	}

	cgi->addNewType(targetType, this, ExprType::TypeAlias);
}






















