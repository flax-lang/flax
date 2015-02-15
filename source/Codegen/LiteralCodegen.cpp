// LiteralCodegen.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "../include/ast.h"
#include "../include/codegen.h"
#include "../include/llvm_all.h"

using namespace Ast;
using namespace Codegen;

Result_t Number::codegen(CodegenInstance* cgi, llvm::Value* lhsPtr)
{
	// check builtin type
	if(!this->decimal)
	{
		int bits = 64;
		if(this->type == "Uint32" || this->type == "Int32")
			bits = 32;

		return Result_t(llvm::ConstantInt::get(cgi->getContext(), llvm::APInt(bits, this->ival, !(this->type[0] == 'U'))), 0);
	}
	else
	{
		return Result_t(llvm::ConstantFP::get(cgi->getContext(), llvm::APFloat(this->dval)), 0);
	}
}

Result_t BoolVal::codegen(CodegenInstance* cgi, llvm::Value* lhsPtr)
{
	return Result_t(llvm::ConstantInt::get(cgi->getContext(), llvm::APInt(1, this->val, false)), 0);
}

Result_t StringLiteral::codegen(CodegenInstance* cgi, llvm::Value* lhsPtr)
{
	llvm::Value* alloca = nullptr;

	if(lhsPtr)	alloca = lhsPtr;
	else		alloca = cgi->mainBuilder.CreateAlloca(cgi->stringType);

	llvm::Value* lengthPtr = cgi->mainBuilder.CreateStructGEP(alloca, 0);
	llvm::Value* stringPtr = cgi->mainBuilder.CreateStructGEP(alloca, 1);

	llvm::Value* lengthVal = llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(cgi->getContext()), this->str.length());
	llvm::Value* stringVal = cgi->mainBuilder.CreateGlobalStringPtr(this->str);

	cgi->mainBuilder.CreateStore(lengthVal, lengthPtr);
	cgi->mainBuilder.CreateStore(stringVal, stringPtr);

	llvm::Value* val = cgi->mainBuilder.CreateLoad(alloca);
	return Result_t(val, alloca);
}



























