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
	Result_t operatorCustom(CodegenInstance* cgi, ArithmeticOp op, Expr* user, std::vector<Expr*> args)
	{
		fir::Value* lhs = 0; fir::Value* lhsptr = 0;
		fir::Value* rhs = 0; fir::Value* rhsptr = 0;

		std::tie(lhs, lhsptr) = args[0]->codegen(cgi);
		std::tie(rhs, rhsptr) = args[1]->codegen(cgi);

		auto data = cgi->getBinaryOperatorOverload(user, op, lhs->getType(), rhs->getType());
		if(data.found)
		{
			return cgi->callBinaryOperatorOverload(data, lhs, lhsptr, rhs, rhsptr, op);
		}
		else
		{
			error(user, "No such operator '%s' for expression %s %s %s", Parser::arithmeticOpToString(cgi, op).c_str(),
				lhs->getType()->str().c_str(), Parser::arithmeticOpToString(cgi, op).c_str(), rhs->getType()->str().c_str());
		}
	}



	Result_t operatorCast(CodegenInstance* cgi, ArithmeticOp op, Expr* user, std::vector<Expr*> args)
	{
		if(args.size() != 2)
			error(user, "Expected 2 arguments for operator %s", Parser::arithmeticOpToString(cgi, op).c_str());

		fir::Value* lhs = 0; fir::Value* lhsptr = 0; ValueKind vk;
		std::tie(lhs, lhsptr, vk) = args[0]->codegen(cgi);

		fir::Type* rtype = args[1]->getType(cgi);
		iceAssert(rtype);


		if(lhs->getType() == rtype)
		{
			warn(user, "Redundant cast (type '%s')", rtype->str().c_str());
			return Result_t(lhs, 0);
		}


		// array constants do not come with a pointer
		if(lhs->getType()->isArrayType() && lhsptr == 0)
			lhsptr = cgi->irb.CreateImmutStackAlloc(lhs->getType(), lhs);



		if(lhs->getType()->isIntegerType() && rtype->isIntegerType())
		{
			return Result_t(cgi->irb.CreateIntSizeCast(lhs, rtype), 0);
		}
		else if(lhs->getType()->isIntegerType() && rtype->isFloatingPointType())
		{
			return Result_t(cgi->irb.CreateIntToFloatCast(lhs, rtype), 0);
		}
		else if(lhs->getType()->isFloatingPointType() && rtype->isFloatingPointType())
		{
			// printf("float to float: %d -> %d\n", lhs->getType()->getPrimitiveSizeInBits(), rtype->getPrimitiveSizeInBits());

			if(lhs->getType()->toPrimitiveType()->getFloatingPointBitWidth() > rtype->toPrimitiveType()->getFloatingPointBitWidth())
				return Result_t(cgi->irb.CreateFTruncate(lhs, rtype), 0);

			else
				return Result_t(cgi->irb.CreateFExtend(lhs, rtype), 0);
		}
		else if(lhs->getType()->isFloatingPointType() && rtype->isIntegerType())
		{
			// truncate
			return Result_t(cgi->irb.CreateFloatToIntCast(lhs, rtype), 0);
		}
		else if(lhs->getType()->isPointerType() && rtype->isPointerType())
		{
			return Result_t(cgi->irb.CreatePointerTypeCast(lhs, rtype), 0);
		}
		else if(lhs->getType()->isPointerType() && rtype->isIntegerType())
		{
			return Result_t(cgi->irb.CreatePointerToIntCast(lhs, rtype), 0);
		}
		else if(lhs->getType()->isIntegerType() && rtype->isPointerType())
		{
			return Result_t(cgi->irb.CreateIntToPointerCast(lhs, rtype), 0);
		}
		else if(lhs->getType()->isArrayType() && rtype->isPointerType()
			&& lhs->getType()->toArrayType()->getElementType() == rtype->getPointerElementType())
		{
			// array to pointer cast.
			iceAssert(lhsptr);

			fir::Value* lhsRawPtr = cgi->irb.CreateConstGEP2(lhsptr, 0, 0);
			return Result_t(lhsRawPtr, 0);
		}
		else if(lhs->getType()->isArrayType() && rtype->isVoidPointer())
		{
			// array to void* cast.
			iceAssert(lhsptr);

			fir::Value* lhsRawPtr = cgi->irb.CreateConstGEP2(lhsptr, 0, 0);
			fir::Value* vptr = cgi->irb.CreatePointerTypeCast(lhsRawPtr, fir::Type::getVoid()->getPointerTo());
			return Result_t(vptr, 0);
		}
		else if(lhs->getType()->isStringType() && rtype->isCharType())
		{
			if(StringLiteral* sl = dynamic_cast<StringLiteral*>(args[0]))
			{
				if(sl->str.size() != 1)
					error(user, "Only single-character string literals can be cast into chars");

				char c = sl->str[0];
				fir::Value* cc = fir::ConstantChar::get(c);

				return Result_t(cc, 0);
			}
			else
			{
				error(user, "Non-literal strings cannot be cast into chars; did you mean to subscript?");
			}
		}
		else if(lhs->getType()->isCharType() && rtype->isIntegerType())
		{
			if(rtype != fir::Type::getInt8())
			{
				fir::Value* ret = cgi->irb.CreateIntSizeCast(cgi->irb.CreateBitcast(lhs, fir::Type::getInt8()), rtype);
				if(ret->getType()->isSignedIntType() != rtype->isSignedIntType())
					ret = cgi->irb.CreateIntSignednessCast(ret, rtype);

				return Result_t(ret, 0);
			}
			else
			{
				return Result_t(cgi->irb.CreateBitcast(lhs, rtype), 0);
			}
		}
		else if(lhs->getType()->isIntegerType() && rtype->isCharType())
		{
			// truncate if required
			if(lhs->getType() != fir::Type::getInt8() && lhs->getType() != fir::Type::getUint8())
				error(user, "Invalid cast to char type from non-i8 type integer (have '%s')", lhs->getType()->str().c_str());

			if(lhs->getType() == fir::Type::getUint8())
				lhs = cgi->irb.CreateIntSignednessCast(lhs, fir::Type::getInt8());

			return Result_t(cgi->irb.CreateBitcast(lhs, fir::Type::getCharType()), 0);
		}
		else if(lhs->getType()->isDynamicArrayType() && rtype->isPointerType() &&
			lhs->getType()->toDynamicArrayType()->getElementType() == rtype->getPointerElementType())
		{
			iceAssert(lhsptr);
			return Result_t(cgi->irb.CreateGetDynamicArrayData(lhsptr), 0);
		}
		else if(rtype->isEnumType() && cgi->getAutoCastDistance(lhs->getType(), rtype->toEnumType()->getCaseType()) >= 0)
		{
			fir::Value* v = cgi->autoCastType(rtype->toEnumType()->getCaseType(), lhs);
			return Result_t(cgi->irb.CreateBitcast(v, rtype), 0);
		}
		else if(lhs->getType()->isEnumType() && cgi->getAutoCastDistance(lhs->getType()->toEnumType()->getCaseType(), rtype) >= 0)
		{
			fir::Value* cval = cgi->irb.CreateBitcast(lhs, lhs->getType()->toEnumType()->getCaseType());

			fir::Value* v = cgi->autoCastType(rtype, cval);
			return Result_t(v, 0);
		}
		else if(lhs->getType()->isAnyType())
		{
			iceAssert(lhsptr);
			return cgi->extractValueFromAny(user, lhsptr, rtype);
		}
		else if(rtype->isAnyType())
		{
			return cgi->makeAnyFromValue(user, lhs, lhsptr, vk);
		}
		else if(op != ArithmeticOp::ForcedCast)
		{
			error(user, "Invalid cast from type '%s' to '%s'", lhs->getType()->str().c_str(), rtype->str().c_str());
		}

		return Result_t(cgi->irb.CreateBitcast(lhs, rtype), 0);
	}
}






