// LiteralCodegen.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "../include/ast.h"
#include "../include/codegen.h"



using namespace Ast;
using namespace Codegen;

Result_t Number::codegen(CodegenInstance* cgi, llvm::Value* lhsPtr, llvm::Value* rhs)
{
	// check builtin type
	if(!this->decimal)
	{
		int bits = 64;
		if(this->type.strType == "Uint32" || this->type.strType == "Int32")
			bits = 32;

		return Result_t(llvm::ConstantInt::get(cgi->getContext(), llvm::APInt(bits, this->ival, !(this->type.strType[0] == 'U'))), 0);
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


		llvm::Value* stringPtr = cgi->builder.CreateStructGEP(alloca, 0);
		llvm::Value* allocdPtr = cgi->builder.CreateStructGEP(alloca, 1);

		llvm::Value* stringVal = cgi->builder.CreateGlobalStringPtr(this->str);

		cgi->builder.CreateStore(stringVal, stringPtr);
		cgi->builder.CreateStore(llvm::ConstantInt::get(llvm::IntegerType::getInt64Ty(cgi->getContext()), 0), allocdPtr);

		llvm::Value* val = cgi->builder.CreateLoad(alloca);
		return Result_t(val, alloca);
	}
	else
	{
		// todo: dirty.
		static bool didWarn = false;

		if(!this->isRaw && !didWarn)
		{
			warn(this, "String type not available, using Int8* for string literal (will not warn again)");
			didWarn = true;
		}

		// good old Int8*
		llvm::Value* stringVal = cgi->builder.CreateGlobalStringPtr(this->str);
		return Result_t(stringVal, 0);
	}
}


Result_t ArrayLiteral::codegen(CodegenInstance* cgi, llvm::Value* lhsPtr, llvm::Value* rhs)
{
	llvm::Type* tp = 0;
	std::vector<llvm::Constant*> vals;

	if(this->values.size() == 0)
	{
		if(!lhsPtr)
		{
			error(cgi, this, "Unable to infer type for empty array");
		}

		tp = lhsPtr->getType()->getPointerElementType();
	}
	else
	{
		tp = cgi->getLlvmType(this->values.front());

		for(Expr* e : this->values)
		{
			llvm::Value* v = e->codegen(cgi).result.first;
			if(llvm::isa<llvm::Constant>(v))
			{
				llvm::Constant* c = llvm::cast<llvm::Constant>(v);

				vals.push_back(c);
				if(vals.back()->getType() != tp)
				{
					error(cgi, e, "Array members must have the same type, got %s and %s",
						cgi->getReadableType(tp).c_str(), cgi->getReadableType(vals.back()->getType()).c_str());
				}
			}
			else
			{
				error(cgi, e, "Array literal members must be constant");
			}
		}
	}

	llvm::ArrayType* atype = llvm::ArrayType::get(tp, this->values.size());
	llvm::Value* alloc = cgi->builder.CreateAlloca(atype);
	llvm::Value* val = llvm::ConstantArray::get(atype, vals);

	cgi->builder.CreateStore(val, alloc);
	return Result_t(val, alloc);
}
