// Arithmetic.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "codegen.h"
#include "operators.h"
#include "runtimefuncs.h"

using namespace Ast;
using namespace Codegen;

fir::Type* getBinOpResultType(CodegenInstance* cgi, BinOp* user, ArithmeticOp op, fir::Type* ltype,
	fir::Type* rtype, fir::Value* extra, bool allowFail);

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







	static Result_t performGeneralArithmeticOperator(CodegenInstance* cgi, ArithmeticOp op, Expr* user, fir::Value* lhs, fir::Value* lhsptr,
		fir::Value* rhs, fir::Value* rhsptr, std::deque<Expr*> exprs)
	{
		auto opstr = Parser::arithmeticOpToString(cgi, op);



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
				error(user, "Operator '%s' cannot be applied on types '%s' and '%s'", opstr.c_str(),
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

			if(isComparisonOp(op))
			{
				return compareIntegers(cgi, op, c1, c2);
			}
			else if(op != ArithmeticOp::Add && op != ArithmeticOp::Subtract)
			{
				error(user, "Invalid operation '%s' between two char types (only '+' and '-' valid)", opstr.c_str());
			}
			else
			{
				fir::Value* res = 0;
				if(op == ArithmeticOp::Add)
					res = cgi->irb.CreateAdd(c1, c2);

				else
					res = cgi->irb.CreateSub(c1, c2);

				return Result_t(cgi->irb.CreateBitcast(res, fir::Type::getCharType()), 0);
			}
		}
		else if(lhs->getType()->isCharType() && rhs->getType()->isIntegerType())
		{
			fir::Value* c1 = cgi->irb.CreateBitcast(lhs, fir::Type::getInt8());
			fir::Value* c2 = cgi->autoCastType(fir::Type::getInt8(), rhs);

			if(c2->getType()->toPrimitiveType()->getIntegerBitWidth() > 8)
			{
				warn(user, "Arithmetic between char type and '%s' may overflow", rhs->getType()->str().c_str());
				c2 = cgi->irb.CreateIntTruncate(c2, fir::Type::getInt8());
			}

			if(op != ArithmeticOp::Add && op != ArithmeticOp::Subtract)
			{
				error(user, "Invalid operation '%s' between char and integer types (only '+' and '-' valid)", opstr.c_str());
			}
			else
			{
				fir::Value* res = 0;
				if(op == ArithmeticOp::Add)
					res = cgi->irb.CreateAdd(c1, c2);

				else
					res = cgi->irb.CreateSub(c1, c2);

				return Result_t(cgi->irb.CreateBitcast(res, fir::Type::getCharType()), 0);
			}
		}
		else if(lhs->getType()->isIntegerType() && rhs->getType()->isCharType())
		{
			fir::Value* c1 = cgi->autoCastType(fir::Type::getInt8(), lhs);
			fir::Value* c2 = cgi->irb.CreateBitcast(rhs, fir::Type::getInt8());


			if(c1->getType()->toPrimitiveType()->getIntegerBitWidth() > 8)
			{
				warn(user, "Arithmetic between char type and '%s' may overflow", lhs->getType()->str().c_str());
				c1 = cgi->irb.CreateIntTruncate(c2, fir::Type::getInt8());
			}


			if(op != ArithmeticOp::Add && op != ArithmeticOp::Subtract)
			{
				error(user, "Invalid operation '%s' between char and integer types (only '+' and '-' valid)", opstr.c_str());
			}
			else
			{
				fir::Value* res = 0;
				if(op == ArithmeticOp::Add)
					res = cgi->irb.CreateAdd(c1, c2);

				else
					res = cgi->irb.CreateSub(c1, c2);

				return Result_t(cgi->irb.CreateBitcast(res, fir::Type::getCharType()), 0);
			}
		}
		else if(lhs->getType()->isStringType() && rhs->getType()->isCharType())
		{
			if(op != ArithmeticOp::Add)
				error(user, "Operator '%s' cannot be applied on types 'string' and 'char'", opstr.c_str());

			iceAssert(rhs);

			// newStringByAppendingChar (does not modify lhsptr)
			auto apf = RuntimeFuncs::String::getCharAppendFunction(cgi);
			fir::Value* app = cgi->irb.CreateCall2(apf, lhs, rhs);
			cgi->addRefCountedValue(app);

			return Result_t(app, 0);
		}
		else if(lhs->getType()->isStringType() && rhs->getType()->isStringType())
		{
			// yay, builtin string operators.
			if(!isComparisonOp(op) && op != ArithmeticOp::Add)
				error(user, "Operator '%s' cannot be applied on two strings", opstr.c_str());

			if(isComparisonOp(op))
			{
				// compare two strings
				iceAssert(lhs);
				iceAssert(rhs);

				fir::Function* cmpf = RuntimeFuncs::String::getCompareFunction(cgi);
				iceAssert(cmpf);

				fir::Value* val = cgi->irb.CreateCall2(cmpf, lhs, rhs);

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
				// newStringByAppendingString (does not modify lhsptr)
				auto apf = RuntimeFuncs::String::getAppendFunction(cgi);
				fir::Value* app = cgi->irb.CreateCall2(apf, lhs, rhs);

				cgi->addRefCountedValue(app);
				return Result_t(app, 0);
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
						arrtype->str().c_str(), opstr.c_str(), arrtype->getElementType()->str().c_str());
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
			iceAssert(lhsptr);

			fir::DynamicArrayType* arrtype = lhs->getType()->toDynamicArrayType();
			if(op == ArithmeticOp::Add)
			{
				// first, clone the left side
				fir::Value* cloned = cgi->irb.CreateStackAlloc(arrtype);
				{
					fir::Function* clonefunc = RuntimeFuncs::Array::getCloneFunction(cgi, arrtype);
					iceAssert(clonefunc);

					cgi->irb.CreateStore(cgi->irb.CreateCall1(clonefunc, lhsptr), cloned);
				}

				// ok, now, append to the clone
				fir::Function* appendf = RuntimeFuncs::Array::getElementAppendFunction(cgi, arrtype);
				cgi->irb.CreateCall2(appendf, cloned, rhs);

				// appended -- return
				return Result_t(cgi->irb.CreateLoad(cloned), cloned);
			}
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
				error(user, "Invalid operator '%s' between types '%s' and '%s'", opstr.c_str(),
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
					opstr.c_str(), lhs->getType()->str().c_str(), caset->str().c_str());
			}
		}
		else if(lhs->getType()->isTupleType() && rhs->getType() == lhs->getType())
		{
			// do each.
			fir::Type* restype = getBinOpResultType(cgi, dynamic_cast<BinOp*>(user), op, lhs->getType(), rhs->getType(), 0, 0);
			iceAssert(restype);

			fir::Value* retval = cgi->irb.CreateStackAlloc(restype);
			cgi->irb.CreateStore(fir::ConstantValue::getNullValue(restype), retval);

			if(!lhsptr)	lhsptr = cgi->irb.CreateImmutStackAlloc(lhs->getType(), lhs);
			if(!rhsptr)	rhsptr = cgi->irb.CreateImmutStackAlloc(rhs->getType(), rhs);






			// for comparisons:
			if(op == ArithmeticOp::CmpLT || op == ArithmeticOp::CmpGT || op == ArithmeticOp::CmpLEq
				|| op == ArithmeticOp::CmpGEq || op == ArithmeticOp::CmpEq || op == ArithmeticOp::CmpNEq)
			{
				iceAssert(retval->getType()->getPointerElementType() == fir::Type::getBool());
				cgi->irb.CreateStore(fir::ConstantInt::getBool(true), retval);

				for(size_t i = 0; i < lhs->getType()->toTupleType()->getElements().size(); i++)
				{
					fir::Value* le = cgi->irb.CreateLoad(cgi->irb.CreateStructGEP(lhsptr, i));
					fir::Value* re = cgi->irb.CreateLoad(cgi->irb.CreateStructGEP(rhsptr, i));

					fir::Value* res = performGeneralArithmeticOperator(cgi, op, exprs.size() > 0 ? exprs[i] : 0, le, 0, re, 0, { }).value;
					iceAssert(res);
					iceAssert(res->getType() == fir::Type::getBool());

					// todo: provide additional information in case of error
					// like uh
					// note: in element 5 of tuple '$T'
					// something like that

					// ok. do bitwise and.
					cgi->irb.CreateBitwiseXOR(fir::ConstantValue::getNullValue(fir::Type::getInt64()), fir::ConstantValue::getNullValue(fir::Type::getInt64()));

					cgi->irb.CreateStore(cgi->irb.CreateBitwiseAND(res, cgi->irb.CreateLoad(retval)), retval);
				}
			}
			else
			{
				// do each element individually
				iceAssert(retval->getType()->isTupleType());
				iceAssert(lhs->getType()->toTupleType()->getElementCount() == rhs->getType()->toTupleType()->getElementCount());

				for(size_t i = 0; i < lhs->getType()->toTupleType()->getElements().size(); i++)
				{
					fir::Value* gep = cgi->irb.CreateStructGEP(retval, i);

					fir::Value* lep = cgi->irb.CreateStructGEP(lhsptr, i);
					fir::Value* lev = cgi->irb.CreateLoad(lep);

					fir::Value* rep = cgi->irb.CreateStructGEP(rhsptr, i);
					fir::Value* rev = cgi->irb.CreateLoad(rep);


					// ok.
					// same thing here, provide more info if we can
					fir::Value* result = performGeneralArithmeticOperator(cgi, op, exprs.size() > 0 ? exprs[i] : 0, lep,
						lev, rep, rev, { }).value;

					iceAssert(result);
					iceAssert(result->getType() == gep->getType()->getPointerElementType());

					// store.
					cgi->irb.CreateStore(result, gep);
				}
			}


			return Result_t(cgi->irb.CreateLoad(retval), 0);
		}
		else
		{
			auto data = cgi->getBinaryOperatorOverload(user, op, lhs->getType(), rhs->getType());
			if(data.found)
			{
				return cgi->callBinaryOperatorOverload(data, lhs, lhsptr, rhs, rhsptr, op);
			}
		}

		error(user, "Operator '%s' cannot be applied in expression %s %s %s", opstr.c_str(),
			lhs->getType()->str().c_str(), opstr.c_str(), rhs->getType()->str().c_str());
	}


	Result_t generalArithmeticOperator(CodegenInstance* cgi, ArithmeticOp op, Expr* user, std::deque<Expr*> args)
	{
		auto opstr = Parser::arithmeticOpToString(cgi, op);

		if(args.size() != 2)
			error(user, "Expected 2 arguments for operator %s", Parser::arithmeticOpToString(cgi, op).c_str());

		fir::Value* lhs = 0; fir::Value* lhsptr = 0;
		fir::Value* rhs = 0; fir::Value* rhsptr = 0;

		std::tie(lhs, lhsptr) = args[0]->codegen(cgi);
		std::tie(rhs, rhsptr) = args[1]->codegen(cgi);

		rhs = cgi->autoCastType(lhs, rhs);

		return performGeneralArithmeticOperator(cgi, op, user, lhs, lhsptr, rhs, rhsptr, args);
	}
}















