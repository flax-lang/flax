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

		auto leftr = args[0]->codegen(cgi);
		auto rightr = args[1]->codegen(cgi);

		fir::Value* lhs = leftr.value;
		fir::Value* rhs = rightr.value;

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

				fir::Value* lhsptr = leftr.pointer;
				fir::Value* rhsptr = rightr.pointer;

				iceAssert(lhsptr);
				iceAssert(rhsptr);


				fir::Function* cmpf = cgi->getStringCompareFunction();
				iceAssert(cmpf);

				fir::Value* val = cgi->irb.CreateCall2(cmpf, lhsptr, rhsptr);

				// we need to convert the int return into booleans
				// if ret < 0, then a < b and a <= b should be 1, and the rest be 0
				// if ret == 0, then a == b, a <= b, and a >= b should be 1, and the rest be 0
				// if ret > 0, then a > b and a >= b should be 1, and the rest be 0

				// basically we just compare the return value to 0 using the same operator.
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
				iceAssert(leftr.pointer);
				iceAssert(rightr.pointer);

				fir::Value* newstrp = cgi->irb.CreateStackAlloc(fir::Type::getStringType());

				auto apf = cgi->getStringAppendFunction();
				fir::Value* app = cgi->irb.CreateCall2(apf, leftr.pointer, rightr.pointer);
				cgi->irb.CreateStore(app, newstrp);

				cgi->addRefCountedValue(newstrp);
				return Result_t(app, newstrp);
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
			return compareEnumValues(cgi, op, lhs, leftr.pointer, rhs, rightr.pointer);
		}
		// else if(lhs->getType()->isStructType() || rhs->getType()->isStructType()
			// || lhs->getType()->isClassType() || rhs->getType()->isClassType())
		else
		{
			auto data = cgi->getBinaryOperatorOverload(user, op, lhs->getType(), rhs->getType());
			if(data.found)
			{
				return cgi->callBinaryOperatorOverload(data, lhs, leftr.pointer, rhs, rightr.pointer, op);
			}
			else
			{
				error(user, "No such operator '%s' for expression %s %s %s", Parser::arithmeticOpToString(cgi, op).c_str(),
					lhs->getType()->str().c_str(), Parser::arithmeticOpToString(cgi, op).c_str(), rhs->getType()->str().c_str());
			}
		}
		// else
		// {
		// 	error(user, "Unsupported operator '%s' on types '%s' and '%s'", Parser::arithmeticOpToString(cgi, op).c_str(),
		// 		lhs->getType()->str().c_str(), rhs->getType()->str().c_str());
		// }
	}
}















