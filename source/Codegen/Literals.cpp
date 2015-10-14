// LiteralCodegen.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "../include/ast.h"
#include "../include/codegen.h"



using namespace Ast;
using namespace Codegen;

Result_t Number::codegen(CodegenInstance* cgi, fir::Value* lhsPtr, fir::Value* rhs)
{
	// check builtin type
	if(!this->decimal)
	{
		int bits = 64;
		if(this->type.strType == "Uint32" || this->type.strType == "Int32")
			bits = 32;

		return Result_t(fir::ConstantInt::get(cgi->getContext(), fir::APInt(bits, this->ival, !(this->type.strType[0] == 'U'))), 0);
	}
	else
	{
		return Result_t(fir::ConstantFP::get(cgi->getContext(), fir::APFloat(this->dval)), 0);
	}
}

Result_t BoolVal::codegen(CodegenInstance* cgi, fir::Value* lhsPtr, fir::Value* rhs)
{
	return Result_t(fir::ConstantInt::get(cgi->getContext(), fir::APInt(1, this->val, false)), 0);
}

Result_t StringLiteral::codegen(CodegenInstance* cgi, fir::Value* lhsPtr, fir::Value* rhs)
{
	auto pair = cgi->getType(cgi->mangleWithNamespace("String", std::deque<std::string>()));
	if(pair && !this->isRaw)
	{
		fir::StructType* stringType = fir::cast<fir::StructType>(pair->first);

		fir::Value* alloca = cgi->allocateInstanceInBlock(stringType);

		// String layout:
		// var data: Int8*
		// var allocated: Uint64


		fir::Value* stringPtr = cgi->builder.CreateStructGEP(alloca, 0);
		fir::Value* allocdPtr = cgi->builder.CreateStructGEP(alloca, 1);

		fir::Value* stringVal = cgi->builder.CreateGlobalStringPtr(this->str);

		cgi->builder.CreateStore(stringVal, stringPtr);
		cgi->builder.CreateStore(fir::ConstantInt::get(fir::IntegerType::getInt64Ty(cgi->getContext()), 0), allocdPtr);

		fir::Value* val = cgi->builder.CreateLoad(alloca);
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
		fir::Value* stringVal = cgi->builder.CreateGlobalStringPtr(this->str);
		return Result_t(stringVal, 0);
	}
}


Result_t ArrayLiteral::codegen(CodegenInstance* cgi, fir::Value* lhsPtr, fir::Value* rhs)
{
	fir::Type* tp = 0;
	std::vector<fir::Constant*> vals;

	if(this->values.size() == 0)
	{
		if(!lhsPtr)
		{
			error(this, "Unable to infer type for empty array");
		}

		tp = lhsPtr->getType()->getPointerElementType();
	}
	else
	{
		tp = cgi->getLlvmType(this->values.front());

		for(Expr* e : this->values)
		{
			fir::Value* v = e->codegen(cgi).result.first;
			if(fir::isa<fir::Constant>(v))
			{
				fir::Constant* c = fir::cast<fir::Constant>(v);

				vals.push_back(c);
				if(vals.back()->getType() != tp)
				{
					error(e, "Array members must have the same type, got %s and %s",
						cgi->getReadableType(tp).c_str(), cgi->getReadableType(vals.back()->getType()).c_str());
				}
			}
			else
			{
				error(e, "Array literal members must be constant");
			}
		}
	}

	fir::ArrayType* atype = fir::ArrayType::get(tp, this->values.size());
	fir::Value* alloc = cgi->builder.CreateAlloca(atype);
	fir::Value* val = fir::ConstantArray::get(atype, vals);

	cgi->builder.CreateStore(val, alloc);
	return Result_t(val, alloc);
}
