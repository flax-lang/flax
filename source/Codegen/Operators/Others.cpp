// Others.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "codegen.h"
#include "operators.h"

using namespace Ast;
using namespace Codegen;

namespace Operators
{
	Result_t operatorCustom(CodegenInstance* cgi, ArithmeticOp op, Expr* user, std::deque<Expr*> args)
	{
		ValPtr_t leftVP = args[0]->codegen(cgi).result;
		ValPtr_t rightVP = args[1]->codegen(cgi).result;

		auto data = cgi->getBinaryOperatorOverload(user, op, leftVP.first->getType(), rightVP.first->getType());
		if(data.found)
		{
			return cgi->callBinaryOperatorOverload(data, leftVP.first, leftVP.second, rightVP.first, rightVP.second, op);
		}
		else
		{
			error(user, "No such operator '%s' for expression %s %s %s", Parser::arithmeticOpToString(cgi, op).c_str(),
				cgi->getReadableType(leftVP.first).c_str(), Parser::arithmeticOpToString(cgi, op).c_str(),
				cgi->getReadableType(rightVP.first).c_str());
		}
	}



	Result_t operatorCast(CodegenInstance* cgi, ArithmeticOp op, Expr* user, std::deque<Expr*> args)
	{
		if(args.size() != 2)
			error(user, "Expected 2 arguments for operator %s", Parser::arithmeticOpToString(cgi, op).c_str());


		auto leftVP = args[0]->codegen(cgi).result;
		fir::Value* lhs = leftVP.first;
		fir::Value* lhsPtr = leftVP.second;

		fir::Type* rtype = cgi->getExprType(args[1]);
		if(!rtype)
		{
			GenError::unknownSymbol(cgi, user, args[1]->type.strType, SymbolType::Type);
		}


		iceAssert(rtype);
		if(lhs->getType() == rtype)
		{
			warn(user, "Redundant cast");
			return Result_t(lhs, 0);
		}



		if(lhs->getType()->isIntegerType() && rtype->isIntegerType())
		{
			return Result_t(cgi->builder.CreateIntSizeCast(lhs, rtype), 0);
		}
		else if(lhs->getType()->isIntegerType() && rtype->isFloatingPointType())
		{
			return Result_t(cgi->builder.CreateIntToFloatCast(lhs, rtype), 0);
		}
		else if(lhs->getType()->isFloatingPointType() && rtype->isFloatingPointType())
		{
			// printf("float to float: %d -> %d\n", lhs->getType()->getPrimitiveSizeInBits(), rtype->getPrimitiveSizeInBits());

			if(lhs->getType()->toPrimitiveType()->getFloatingPointBitWidth() > rtype->toPrimitiveType()->getFloatingPointBitWidth())
				return Result_t(cgi->builder.CreateFTruncate(lhs, rtype), 0);

			else
				return Result_t(cgi->builder.CreateFExtend(lhs, rtype), 0);
		}
		else if(lhs->getType()->isPointerType() && rtype->isPointerType())
		{
			return Result_t(cgi->builder.CreatePointerTypeCast(lhs, rtype), 0);
		}
		else if(lhs->getType()->isPointerType() && rtype->isIntegerType())
		{
			return Result_t(cgi->builder.CreatePointerToIntCast(lhs, rtype), 0);
		}
		else if(lhs->getType()->isIntegerType() && rtype->isPointerType())
		{
			return Result_t(cgi->builder.CreateIntToPointerCast(lhs, rtype), 0);
		}
		else if(cgi->isEnum(rtype))
		{
			// dealing with enum
			fir::Type* insideType = rtype->toStructType()->getElementN(0);
			if(lhs->getType() == insideType)
			{
				fir::Value* tmp = cgi->getStackAlloc(rtype, "tmp_enum");

				fir::Value* gep = cgi->builder.CreateStructGEP(tmp, 0);
				cgi->builder.CreateStore(lhs, gep);

				return Result_t(cgi->builder.CreateLoad(tmp), tmp);
			}
			else
			{
				error(user, "Enum '%s' does not have type '%s', invalid cast", rtype->toStructType()->getStructName().str().c_str(),
					lhs->getType()->str().c_str());
			}
		}
		else if(cgi->isEnum(lhs->getType()) && lhs->getType()->toStructType()->getElementN(0) == rtype)
		{
			iceAssert(lhsPtr);
			fir::Value* gep = cgi->builder.CreateStructGEP(lhsPtr, 0);
			fir::Value* val = cgi->builder.CreateLoad(gep);

			return Result_t(val, gep);
		}
		else if(cgi->isAnyType(lhs->getType()))
		{
			iceAssert(lhsPtr);
			return cgi->extractValueFromAny(rtype, lhsPtr);
		}
		else if(lhs->getType()->isClassType() && lhs->getType()->toClassType()->getClassName().str() == "String"
			&& rtype == fir::PointerType::getInt8Ptr(cgi->getContext()))
		{
			// string to int8*.
			// just access the data pointer.

			iceAssert(lhsPtr);
			fir::Value* stringPtr = cgi->builder.CreateStructGEP(lhsPtr, 0);

			return Result_t(cgi->builder.CreateLoad(stringPtr), stringPtr);
		}
		else if(lhs->getType() == fir::PointerType::getInt8Ptr(cgi->getContext())
			&& rtype->isClassType() && rtype->toClassType()->getClassName().str() == "String")
		{
			// support this shit.
			// error(cgi, this, "Automatic char* -> String casting not yet supported");

			// create a bogus func call.
			TypePair_t* tp = cgi->getTypeByString("String");
			iceAssert(tp);

			std::vector<fir::Value*> args { lhs };
			return cgi->callTypeInitialiser(tp, user, args);
		}
		else if(lhs->getType()->isArrayType() && rtype->isPointerType()
			&& lhs->getType()->toArrayType()->getElementType() == rtype->getPointerElementType())
		{
			// array to pointer cast.
			if(!dynamic_cast<fir::ConstantArray*>(lhs))
				warn(user, "Casting from non-constant array to pointer is potentially unsafe");

			iceAssert(lhsPtr);

			fir::Value* lhsRawPtr = cgi->builder.CreateConstGEP2(lhsPtr, 0, 0);
			return Result_t(lhsRawPtr, 0);
		}
		else if(op != ArithmeticOp::ForcedCast)
		{
			std::string lstr = cgi->getReadableType(lhs).c_str();
			std::string rstr = cgi->getReadableType(rtype).c_str();

			error(user, "Invalid cast from type %s to %s", lstr.c_str(), rstr.c_str());
		}

		return Result_t(cgi->builder.CreateBitcast(lhs, rtype), 0);
	}
}






