// LiteralCodegen.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "../include/ast.h"
#include "../include/codegen.h"
#include "../include/llvm_all.h"

using namespace Ast;
using namespace Codegen;

Result_t Number::codegen(CodegenInstance* cgi, llvm::Value* lhsPtr, llvm::Value* rhs)
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

Result_t BoolVal::codegen(CodegenInstance* cgi, llvm::Value* lhsPtr, llvm::Value* rhs)
{
	return Result_t(llvm::ConstantInt::get(cgi->getContext(), llvm::APInt(1, this->val, false)), 0);
}

Result_t StringLiteral::codegen(CodegenInstance* cgi, llvm::Value* lhsPtr, llvm::Value* rhs)
{
	auto pair = cgi->getType(cgi->mangleWithNamespace("String", std::deque<std::string>()));
	if(pair && !this->isRaw)
	{
		llvm::StructType* stringType = llvm::cast<llvm::StructType>(pair->first);

		llvm::Value* alloca = cgi->allocateInstanceInBlock(stringType);

		// String layout:
		// var data: Int8*
		// var allocated: Uint64


		llvm::Value* stringPtr = cgi->mainBuilder.CreateStructGEP(alloca, 0);
		llvm::Value* allocdPtr = cgi->mainBuilder.CreateStructGEP(alloca, 1);

		llvm::Value* stringVal = cgi->mainBuilder.CreateGlobalStringPtr(this->str);

		cgi->mainBuilder.CreateStore(stringVal, stringPtr);
		cgi->mainBuilder.CreateStore(llvm::ConstantInt::get(llvm::IntegerType::getInt64Ty(cgi->getContext()), 0), allocdPtr);

		llvm::Value* val = cgi->mainBuilder.CreateLoad(alloca);
		return Result_t(val, alloca);
	}
	else
	{
		if(!this->isRaw)
			warn(this, "String type not available, using Int8* for string literal");

		// good old Int8*
		llvm::Value* stringVal = cgi->mainBuilder.CreateGlobalStringPtr(this->str);
		return Result_t(stringVal, 0);
	}
}



























