// Arithmetic.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "codegen.h"
#include "operators.h"

using namespace Ast;
using namespace Codegen;

namespace Operators
{
	static bool isComparisonOp(ArithmeticOp op)
	{
		return	(op == ArithmeticOp::CmpEq) ||
				(op == ArithmeticOp::CmpNEq) ||
				(op == ArithmeticOp::CmpLT) ||
				(op == ArithmeticOp::CmpGT) ||
				(op == ArithmeticOp::CmpLEq) ||
				(op == ArithmeticOp::CmpGEq);
	}

	static Result_t compareFloatingPoints(CodegenInstance* cgi, ArithmeticOp op, fir::Value* lhs, fir::Value* rhs)
	{
		switch(op)
		{
			case ArithmeticOp::CmpEq:		return Result_t(cgi->irb.CreateFCmpEQ_ORD(lhs, rhs), 0);
			case ArithmeticOp::CmpNEq:		return Result_t(cgi->irb.CreateFCmpNEQ_ORD(lhs, rhs), 0);
			case ArithmeticOp::CmpLT:		return Result_t(cgi->irb.CreateFCmpLT_ORD(lhs, rhs), 0);
			case ArithmeticOp::CmpGT:		return Result_t(cgi->irb.CreateFCmpGT_ORD(lhs, rhs), 0);
			case ArithmeticOp::CmpLEq:		return Result_t(cgi->irb.CreateFCmpLEQ_ORD(lhs, rhs), 0);
			case ArithmeticOp::CmpGEq:		return Result_t(cgi->irb.CreateFCmpGEQ_ORD(lhs, rhs), 0);

			default:	iceAssert(0);
		}
	}

	static Result_t compareIntegers(CodegenInstance* cgi, ArithmeticOp op, fir::Value* lhs, fir::Value* rhs)
	{
		switch(op)
		{
			case ArithmeticOp::CmpEq:		return Result_t(cgi->irb.CreateICmpEQ(lhs, rhs), 0);
			case ArithmeticOp::CmpNEq:		return Result_t(cgi->irb.CreateICmpNEQ(lhs, rhs), 0);
			case ArithmeticOp::CmpLT:		return Result_t(cgi->irb.CreateICmpLT(lhs, rhs), 0);
			case ArithmeticOp::CmpGT:		return Result_t(cgi->irb.CreateICmpGT(lhs, rhs), 0);
			case ArithmeticOp::CmpLEq:		return Result_t(cgi->irb.CreateICmpLEQ(lhs, rhs), 0);
			case ArithmeticOp::CmpGEq:		return Result_t(cgi->irb.CreateICmpGEQ(lhs, rhs), 0);

			default:	iceAssert(0);
		}
	}

	static Result_t performPointerArithemetic(CodegenInstance* cgi, ArithmeticOp op, fir::Value* lhs, fir::Value* rhs)
	{
		// note: mess above is because
		// APPARENTLY,
		// 1 + foo === foo + 1, even if foo is a pointer.

		// make life easier below.
		if(lhs->getType()->isIntegerType())
			std::swap(lhs, rhs);

		// do the pointer arithmetic thing
		fir::Type* ptrIntType = cgi->execTarget->getPointerSizedIntegerType();

		if(rhs->getType() != ptrIntType)
			rhs = cgi->irb.CreateIntSizeCast(rhs, ptrIntType);

		// do the actual thing.
		fir::Value* ret = 0;
		if(op == ArithmeticOp::Add)	ret = cgi->irb.CreatePointerAdd(lhs, rhs);
		else						ret = cgi->irb.CreatePointerSub(lhs, rhs);

		return Result_t(ret, 0);
	}

	static Result_t compareEnumValues(CodegenInstance* cgi, ArithmeticOp op, fir::Value* lhs, fir::Value* lhsPtr,
		fir::Value* rhs, fir::Value* rhsPtr)
	{
		// enums
		iceAssert(lhsPtr);
		iceAssert(rhsPtr);

		fir::Value* gepL = cgi->irb.CreateStructGEP(lhsPtr, 0);
		fir::Value* gepR = cgi->irb.CreateStructGEP(rhsPtr, 0);

		fir::Value* l = cgi->irb.CreateLoad(gepL);
		fir::Value* r = cgi->irb.CreateLoad(gepR);

		fir::Value* res = 0;

		if(op == ArithmeticOp::CmpEq)
		{
			res = cgi->irb.CreateICmpEQ(l, r);
		}
		else if(op == ArithmeticOp::CmpNEq)
		{
			res = cgi->irb.CreateICmpNEQ(l, r);
		}

		return Result_t(res, 0);
	}






	Result_t generalArithmeticOperator(CodegenInstance* cgi, ArithmeticOp op, Expr* user, std::deque<Expr*> args)
	{
		if(args.size() != 2)
			error(user, "Expected 2 arguments for operator %s", Parser::arithmeticOpToString(cgi, op).c_str());

		ValPtr_t leftVP = args[0]->codegen(cgi).result;
		ValPtr_t rightVP = args[1]->codegen(cgi).result;

		fir::Value* lhs = leftVP.first;
		fir::Value* rhs = rightVP.first;

		rhs = cgi->autoCastType(lhs, rhs);

		if(isComparisonOp(op) && lhs->getType() == rhs->getType() && (lhs->getType()->isPointerType() || lhs->getType()->isPrimitiveType())
								&& (rhs->getType()->isPointerType() || rhs->getType()->isPrimitiveType()))
		{
			// todo(behaviour): c/c++ states that comparing pointers (>, <, etc) is undefined when the pointers do not
			// point to the same "aggregate" (pointer to struct members, or array members)
			// we allow it, but should this is changed?

			// if everyone is primitive, autocasting should have made them the same
			iceAssert(lhs->getType() == rhs->getType());

			if(lhs->getType()->isFloatingPointType() || rhs->getType()->isFloatingPointType())
			{
				return compareFloatingPoints(cgi, op, lhs, rhs);
			}
			else
			{
				return compareIntegers(cgi, op, lhs, rhs);
			}
		}
		else if(lhs->getType()->isStringType() && rhs->getType()->isStringType())
		{
			// yay, builtin string operators.
			if(!isComparisonOp(op) && op != ArithmeticOp::Add)
				error(user, "Operator '%s' cannot be applied on two strings", Parser::arithmeticOpToString(cgi, op).c_str());

			if(isComparisonOp(op))
			{
				// compare two strings

				fir::Value* lhsptr = leftVP.second;
				fir::Value* rhsptr = rightVP.second;

				iceAssert(lhsptr);
				iceAssert(rhsptr);


				fir::Function* cmpf = cgi->getStringCompareFunction();
				iceAssert(cmpf);

				fir::Value* val = cgi->irb.CreateCall2(cmpf, lhsptr, rhsptr);

				// we need to convert the int return into booleans
				// if ret < 0, then a < b and a <= b should be 1, and the rest be 0
				// if ret == 0, then a == b, a <= b, and a >= b should be 1, and the rest be 0
				// if ret > 0, then a > b and a >= b should be 1, and the rest be 0

				// basically we should actually just have separate routines for each operation
				fir::Value* ret = 0;
				auto zero = fir::ConstantInt::getInt64(0);

				if(op == ArithmeticOp::CmpLT)			ret = cgi->irb.CreateICmpLT(val, zero);
				else if(op == ArithmeticOp::CmpLEq)		ret = cgi->irb.CreateICmpLEQ(val, zero);
				else if(op == ArithmeticOp::CmpGT)		ret = cgi->irb.CreateICmpGT(val, zero);
				else if(op == ArithmeticOp::CmpGEq)		ret = cgi->irb.CreateICmpGEQ(val, zero);
				else if(op == ArithmeticOp::CmpEq)		ret = cgi->irb.CreateICmpEQ(val, zero);
				else if(op == ArithmeticOp::CmpNEq)		ret = cgi->irb.CreateICmpNEQ(val, zero);

				iceAssert(ret);
				return Result_t(ret, 0);
			}
			else
			{
				// add two strings
				// steps:
				//
				// 1. get the size of the left string
				// 2. get the size of the right string
				// 3. add them together
				// 4. malloc a string of that size + 1
				// 5. make a new string
				// 6. set the buffer to the malloced buffer
				// 7. set the length to a + b
				// 8. return.

				// get an empty string
				auto r = cgi->getEmptyString();
				fir::Value* newstrp = r.result.second;
				newstrp->setName("newstrp");

				fir::Value* lhsptr = leftVP.second;
				fir::Value* rhsptr = rightVP.second;

				iceAssert(lhsptr);
				iceAssert(rhsptr);

				fir::Value* lhslen = cgi->irb.CreateGetStringLength(lhsptr);
				fir::Value* rhslen = cgi->irb.CreateGetStringLength(rhsptr);

				fir::Value* lhsbuf = cgi->irb.CreateGetStringData(lhsptr);
				fir::Value* rhsbuf = cgi->irb.CreateGetStringData(rhsptr);


				// ok. combine the lengths
				fir::Value* newlen = cgi->irb.CreateAdd(lhslen, rhslen);
				newlen = cgi->irb.CreateAdd(newlen, fir::ConstantInt::getInt32(1));		// space for null

				// now malloc.
				fir::Function* mallocf = cgi->module->getFunction(cgi->getOrDeclareLibCFunc("malloc").firFunc->getName());
				iceAssert(mallocf);

				fir::Value* buf = cgi->irb.CreateCall1(mallocf, cgi->irb.CreateIntSizeCast(newlen, fir::PrimitiveType::getInt64()));

				// now memcpy
				fir::Function* memcpyf = cgi->module->getIntrinsicFunction("memcpy");
				cgi->irb.CreateCall(memcpyf, { buf, lhsbuf, cgi->irb.CreateIntSizeCast(lhslen, fir::PrimitiveType::getInt64()),
					fir::ConstantInt::getInt32(0), fir::ConstantInt::getBool(0) });

				fir::Value* offsetbuf = cgi->irb.CreatePointerAdd(buf, lhslen);
				cgi->irb.CreateCall(memcpyf, { offsetbuf, rhsbuf, cgi->irb.CreateIntSizeCast(rhslen, fir::PrimitiveType::getInt64()),
					fir::ConstantInt::getInt32(0), fir::ConstantInt::getBool(0) });

				// null terminator
				fir::Value* nt = cgi->irb.CreateGetPointer(offsetbuf, rhslen);
				cgi->irb.CreateStore(fir::ConstantInt::getInt8(0), nt);

				// ok, now fix it
				cgi->irb.CreateSetStringData(newstrp, buf);
				cgi->irb.CreateSetStringLength(newstrp, newlen);
				cgi->irb.CreateSetStringRefCount(newstrp, fir::ConstantInt::getInt32(1));

				cgi->addRefCountedValue(newstrp);
				return Result_t(cgi->irb.CreateLoad(newstrp), newstrp);
			}
		}
		else if(lhs->getType()->isPrimitiveType() && rhs->getType()->isPrimitiveType())
		{
			fir::Value* tryop = cgi->irb.CreateBinaryOp(op, lhs, rhs);
			iceAssert(tryop);

			return Result_t(tryop, 0);
		}
		else if((op == ArithmeticOp::Add || op == ArithmeticOp::Subtract) && ((lhs->getType()->isPointerType() && rhs->getType()->isIntegerType())
			|| (lhs->getType()->isIntegerType() && rhs->getType()->isPointerType())))
		{
			return performPointerArithemetic(cgi, op, lhs, rhs);
		}
		else if((op == ArithmeticOp::CmpEq || op == ArithmeticOp::CmpNEq) && cgi->isEnum(lhs->getType()) && cgi->isEnum(rhs->getType()))
		{
			return compareEnumValues(cgi, op, lhs, leftVP.second, rhs, rightVP.second);
		}
		else if(lhs->getType()->isStructType() || rhs->getType()->isStructType()
			|| lhs->getType()->isClassType() || rhs->getType()->isClassType())
		{
			auto data = cgi->getBinaryOperatorOverload(user, op, lhs->getType(), rhs->getType());
			if(data.found)
			{
				return cgi->callBinaryOperatorOverload(data, lhs, leftVP.second, rhs, rightVP.second, op);
			}
			else
			{
				error(user, "No such operator '%s' for expression %s %s %s", Parser::arithmeticOpToString(cgi, op).c_str(),
					lhs->getType()->str().c_str(), Parser::arithmeticOpToString(cgi, op).c_str(), rhs->getType()->str().c_str());
			}
		}
		else
		{
			error(user, "Unsupported operator '%s' on types '%s' and '%s'", Parser::arithmeticOpToString(cgi, op).c_str(),
				lhs->getType()->str().c_str(), rhs->getType()->str().c_str());
		}
	}
}















