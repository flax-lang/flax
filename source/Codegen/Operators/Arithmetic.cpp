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
			case ArithmeticOp::CmpEq:		return Result_t(cgi->builder.CreateFCmpEQ_ORD(lhs, rhs), 0);
			case ArithmeticOp::CmpNEq:		return Result_t(cgi->builder.CreateFCmpNEQ_ORD(lhs, rhs), 0);
			case ArithmeticOp::CmpLT:		return Result_t(cgi->builder.CreateFCmpLT_ORD(lhs, rhs), 0);
			case ArithmeticOp::CmpGT:		return Result_t(cgi->builder.CreateFCmpGT_ORD(lhs, rhs), 0);
			case ArithmeticOp::CmpLEq:		return Result_t(cgi->builder.CreateFCmpLEQ_ORD(lhs, rhs), 0);
			case ArithmeticOp::CmpGEq:		return Result_t(cgi->builder.CreateFCmpGEQ_ORD(lhs, rhs), 0);

			default:	iceAssert(0);
		}
	}

	static Result_t compareIntegers(CodegenInstance* cgi, ArithmeticOp op, fir::Value* lhs, fir::Value* rhs)
	{
		switch(op)
		{
			case ArithmeticOp::CmpEq:		return Result_t(cgi->builder.CreateICmpEQ(lhs, rhs), 0);
			case ArithmeticOp::CmpNEq:		return Result_t(cgi->builder.CreateICmpNEQ(lhs, rhs), 0);
			case ArithmeticOp::CmpLT:		return Result_t(cgi->builder.CreateICmpLT(lhs, rhs), 0);
			case ArithmeticOp::CmpGT:		return Result_t(cgi->builder.CreateICmpGT(lhs, rhs), 0);
			case ArithmeticOp::CmpLEq:		return Result_t(cgi->builder.CreateICmpLEQ(lhs, rhs), 0);
			case ArithmeticOp::CmpGEq:		return Result_t(cgi->builder.CreateICmpGEQ(lhs, rhs), 0);

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
			rhs = cgi->builder.CreateIntSizeCast(rhs, ptrIntType);

		// do the actual thing.
		fir::Value* ret = 0;
		if(op == ArithmeticOp::Add)	ret = cgi->builder.CreatePointerAdd(lhs, rhs);
		else						ret = cgi->builder.CreatePointerSub(lhs, rhs);

		return Result_t(ret, 0);
	}

	static Result_t compareEnumValues(CodegenInstance* cgi, ArithmeticOp op, fir::Value* lhs, fir::Value* lhsPtr,
		fir::Value* rhs, fir::Value* rhsPtr)
	{
		// enums
		iceAssert(lhsPtr);
		iceAssert(rhsPtr);

		fir::Value* gepL = cgi->builder.CreateStructGEP(lhsPtr, 0);
		fir::Value* gepR = cgi->builder.CreateStructGEP(rhsPtr, 0);

		fir::Value* l = cgi->builder.CreateLoad(gepL);
		fir::Value* r = cgi->builder.CreateLoad(gepR);

		fir::Value* res = 0;

		if(op == ArithmeticOp::CmpEq)
		{
			res = cgi->builder.CreateICmpEQ(l, r);
		}
		else if(op == ArithmeticOp::CmpNEq)
		{
			res = cgi->builder.CreateICmpNEQ(l, r);
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
		else if(lhs->getType()->isPrimitiveType() && rhs->getType()->isPrimitiveType())
		{
			fir::Value* tryop = cgi->builder.CreateBinaryOp(op, lhs, rhs);
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
					lhs->getType()->cstr(), Parser::arithmeticOpToString(cgi, op).c_str(), rhs->getType()->cstr());
			}
		}
		else
		{
			error(user, "Unsupported operator '%s' on types %s and %s", Parser::arithmeticOpToString(cgi, op).c_str(),
				lhs->getType()->cstr(), rhs->getType()->cstr());
		}
	}
}















