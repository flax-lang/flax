// Arithmetic.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "codegen.h"
#include "operators.h"
#include "runtimefuncs.h"

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





	Result_t generalArithmeticOperator(CodegenInstance* cgi, ArithmeticOp op, Expr* user, std::deque<Expr*> args)
	{
		auto optostr = Parser::arithmeticOpToString;

		if(args.size() != 2)
			error(user, "Expected 2 arguments for operator %s", Parser::arithmeticOpToString(cgi, op).c_str());

		fir::Value* lhs = 0; fir::Value* lhsptr = 0;
		fir::Value* rhs = 0; fir::Value* rhsptr = 0;

		std::tie(lhs, lhsptr) = args[0]->codegen(cgi);
		std::tie(rhs, rhsptr) = args[1]->codegen(cgi);

		rhs = cgi->autoCastType(lhs, rhs);


		if(isComparisonOp(op) && (lhs->getType()->isPointerType() || lhs->getType()->isPrimitiveType())
								&& (rhs->getType()->isPointerType() || rhs->getType()->isPrimitiveType()))
		{
			// todo(behaviour): c/c++ states that comparing pointers (>, <, etc) is undefined when the pointers do not
			// point to the same "aggregate" (pointer to struct members, or array members)
			// we allow it, but should this is changed?

			// if everyone is primitive, autocasting should have made them the same
			if(lhs->getType() != rhs->getType())
				lhs = cgi->autoCastType(rhs, lhs);

			if(lhs->getType() != rhs->getType())
			{
				error(user, "Operator '%s' cannot be applied on types '%s' and '%s'", optostr(cgi, op).c_str(),
					lhs->getType()->str().c_str(), rhs->getType()->str().c_str());
			}

			if(lhs->getType()->isFloatingPointType() || rhs->getType()->isFloatingPointType())
			{
				return compareFloatingPoints(cgi, op, lhs, rhs);
			}
			else
			{
				return compareIntegers(cgi, op, lhs, rhs);
			}
		}
		else if(lhs->getType()->isCharType() && rhs->getType()->isCharType())
		{
			fir::Value* c1 = cgi->irb.CreateBitcast(lhs, fir::Type::getInt8());
			fir::Value* c2 = cgi->irb.CreateBitcast(rhs, fir::Type::getInt8());

			return compareIntegers(cgi, op, c1, c2);
		}
		else if(lhs->getType()->isStringType() && rhs->getType()->isCharType())
		{
			if(op != ArithmeticOp::Add)
				error(user, "Operator '%s' cannot be applied on types 'string' and 'char'", optostr(cgi, op).c_str());

			iceAssert(lhsptr);
			iceAssert(rhs);

			fir::Value* newstrp = cgi->irb.CreateStackAlloc(fir::Type::getStringType());

			// newStringByAppendingChar (does not modify lhsptr)
			auto apf = RuntimeFuncs::String::getCharAppendFunction(cgi);
			fir::Value* app = cgi->irb.CreateCall2(apf, lhsptr, rhs);
			cgi->irb.CreateStore(app, newstrp);

			cgi->addRefCountedValue(newstrp);
			return Result_t(app, newstrp);
		}
		else if(lhs->getType()->isStringType() && rhs->getType()->isStringType())
		{
			// yay, builtin string operators.
			if(!isComparisonOp(op) && op != ArithmeticOp::Add)
				error(user, "Operator '%s' cannot be applied on two strings", optostr(cgi, op).c_str());

			if(isComparisonOp(op))
			{
				// compare two strings
				iceAssert(lhsptr);
				iceAssert(rhsptr);


				fir::Function* cmpf = RuntimeFuncs::String::getCompareFunction(cgi);
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
				fir::Value* newstrp = cgi->irb.CreateStackAlloc(fir::Type::getStringType());

				// newStringByAppendingString (does not modify lhsptr)
				auto apf = RuntimeFuncs::String::getAppendFunction(cgi);
				fir::Value* app = cgi->irb.CreateCall2(apf, lhsptr, rhsptr);
				cgi->irb.CreateStore(app, newstrp);

				cgi->addRefCountedValue(newstrp);
				return Result_t(app, newstrp);
			}
		}
		else if(lhs->getType()->isDynamicArrayType() && rhs->getType()->isDynamicArrayType()
			&& lhs->getType()->toDynamicArrayType()->getElementType() == rhs->getType()->toDynamicArrayType()->getElementType())
		{
			iceAssert(lhsptr);
			iceAssert(rhsptr);

			fir::DynamicArrayType* arrtype = lhs->getType()->toDynamicArrayType();
			if(isComparisonOp(op))
			{
				// check if we can actually compare the two element types
				auto ovl = cgi->getBinaryOperatorOverload(user, op, arrtype->getElementType(), arrtype->getElementType());
				if(!ovl.found)
				{
					error(user, "Array type '%s' cannot be compared; operator '%s' is not defined for element type '%s'",
						arrtype->str().c_str(), optostr(cgi, op).c_str(), arrtype->getElementType()->str().c_str());
				}

				// basically this calls the elementwise comparison function
				// note: if ovl.opFunc is null, the IR will assume that FIR knows how to handle it
				// ie. builtin type compares
				fir::Function* cmpf = RuntimeFuncs::Array::getCompareFunction(cgi, arrtype, ovl.opFunc);
				iceAssert(cmpf);


				fir::Value* val = cgi->irb.CreateCall2(cmpf, lhsptr, rhsptr);

				// same as the string stuff above
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
			else if(op == ArithmeticOp::Add)
			{
				// first, clone the left side
				fir::Value* cloned = cgi->irb.CreateStackAlloc(arrtype);
				{
					fir::Function* clonefunc = RuntimeFuncs::Array::getCloneFunction(cgi, arrtype);
					iceAssert(clonefunc);

					cgi->irb.CreateStore(cgi->irb.CreateCall1(clonefunc, lhsptr), cloned);
				}

				// ok, now, append to the clone
				fir::Function* appendf = RuntimeFuncs::Array::getAppendFunction(cgi, arrtype);
				cgi->irb.CreateCall2(appendf, cloned, rhsptr);

				// appended -- return
				return Result_t(cgi->irb.CreateLoad(cloned), cloned);
			}
		}
		else if(lhs->getType()->isDynamicArrayType() && lhs->getType()->toDynamicArrayType()->getElementType() == rhs->getType())
		{
			error("nope");
		}
		else if(lhs->getType()->isPrimitiveType() && rhs->getType()->isPrimitiveType())
		{
			if(lhs->getType() != rhs->getType())
				lhs = cgi->autoCastType(rhs, lhs);

			fir::Value* tryop = 0;
			if(lhs->getType() != rhs->getType())
				goto die;

			tryop = cgi->irb.CreateBinaryOp(op, lhs, rhs);
			if(!tryop)
			{
				die:
				error(user, "Invalid operator '%s' between types '%s' and '%s'", optostr(cgi, op).c_str(),
					lhs->getType()->str().c_str(), rhs->getType()->str().c_str());
			}

			return Result_t(tryop, 0);
		}
		else if((op == ArithmeticOp::Add || op == ArithmeticOp::Subtract) && ((lhs->getType()->isPointerType() && rhs->getType()->isIntegerType())
			|| (lhs->getType()->isIntegerType() && rhs->getType()->isPointerType())))
		{
			return performPointerArithemetic(cgi, op, lhs, rhs);
		}
		else if(lhs->getType()->isEnumType() && rhs->getType() == lhs->getType())
		{
			fir::Type* caset = lhs->getType()->toEnumType()->getCaseType();

			// compare enums or something
			if(isComparisonOp(op))
			{
				if(!lhs->getType()->toEnumType()->getCaseType()->isPrimitiveType())
					error(user, "Only enumerations with integer/floating-point types can be compared, have '%s'", caset->str().c_str());

				fir::Value* l = cgi->irb.CreateBitcast(lhs, caset);
				fir::Value* r = cgi->irb.CreateBitcast(rhs, caset);

				if(caset->isFloatingPointType())	return compareFloatingPoints(cgi, op, l, r);
				else								return compareIntegers(cgi, op, l, r);
			}
			else if(op == ArithmeticOp::BitwiseAnd || op == ArithmeticOp::BitwiseOr || op == ArithmeticOp::BitwiseXor
				|| op == ArithmeticOp::BitwiseNot)
			{
				// support this common case of using enums as flags
				if(!lhs->getType()->toEnumType()->getCaseType()->isIntegerType())
					error(user, "Bitwise operations on enumerations are only valid with integer types, have '%s'", caset->str().c_str());

				fir::Value* l = cgi->irb.CreateBitcast(lhs, caset);
				fir::Value* r = cgi->irb.CreateBitcast(rhs, caset);

				fir::Value* tryop = cgi->irb.CreateBinaryOp(op, l, r);
				iceAssert(tryop);

				return Result_t(cgi->irb.CreateBitcast(tryop, lhs->getType()), 0);
			}
			else
			{
				error(user, "Invalid operator '%s' between enumeration types '%s' (underlying type '%s')",
					optostr(cgi, op).c_str(), lhs->getType()->str().c_str(), caset->str().c_str());
			}
		}
		else
		{
			auto data = cgi->getBinaryOperatorOverload(user, op, lhs->getType(), rhs->getType());
			if(data.found)
			{
				return cgi->callBinaryOperatorOverload(data, lhs, lhsptr, rhs, rhsptr, op);
			}
		}

		error(user, "No such operator '%s' for expression %s %s %s", optostr(cgi, op).c_str(),
			lhs->getType()->str().c_str(), optostr(cgi, op).c_str(), rhs->getType()->str().c_str());
	}
}















