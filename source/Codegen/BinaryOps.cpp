// ExprCodeGen.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include <cinttypes>

#include "ast.h"
#include "codegen.h"
#include "parser.h"

using namespace Ast;
using namespace Codegen;



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
	else if(lhs->getType()->isStructType() || rhs->getType()->isStructType())
	{
		auto data = cgi->getOperatorOverload(user, op, lhs->getType(), rhs->getType());
		Result_t ret = cgi->callOperatorOverload(data, lhs, leftVP.second, rhs, rightVP.second, op);

		if(ret.result.first == 0)
		{
			error(user, "No such operator '%s' for expression %s %s %s", Parser::arithmeticOpToString(cgi, op).c_str(),
				cgi->getReadableType(lhs).c_str(), Parser::arithmeticOpToString(cgi, op).c_str(), cgi->getReadableType(rhs).c_str());
		}

		return ret;
	}
	else
	{
		error(user, "Unsupported operator '%s' on types %s and %s", Parser::arithmeticOpToString(cgi, op).c_str(),
			lhs->getType()->str().c_str(), rhs->getType()->str().c_str());

		// return Result_t(0, 0);
	}
}




Result_t operatorAssign(CodegenInstance* cgi, ArithmeticOp op, Expr* user, std::deque<Expr*> args)
{
	if(args.size() != 2)
		error(user, "Expected 2 arguments for operator %s", Parser::arithmeticOpToString(cgi, op).c_str());

	return Result_t(0, 0);
}




Result_t generalCompoundAssignOperator(CodegenInstance* cgi, ArithmeticOp op, Expr* user, std::deque<Expr*> args)
{
	if(args.size() != 2)
		error(user, "Expected 2 arguments for operator %s", Parser::arithmeticOpToString(cgi, op).c_str());

	return Result_t(0, 0);
}









































Result_t operatorLogicalNot(CodegenInstance* cgi, ArithmeticOp op, Expr* user, std::deque<Expr*> args)
{
	if(args.size() != 2)
		error(user, "Expected 2 arguments for operator %s", Parser::arithmeticOpToString(cgi, op).c_str());

	return Result_t(0, 0);
}

Result_t operatorCast(CodegenInstance* cgi, ArithmeticOp op, Expr* user, std::deque<Expr*> args)
{
	if(args.size() != 2)
		error(user, "Expected 2 arguments for operator %s", Parser::arithmeticOpToString(cgi, op).c_str());

	return Result_t(0, 0);
}








Result_t operatorLogicalAnd(CodegenInstance* cgi, ArithmeticOp op, Expr* user, std::deque<Expr*> args)
{
	if(args.size() != 2)
		error(user, "Expected 2 arguments for operator %s", Parser::arithmeticOpToString(cgi, op).c_str());

	// get ourselves some short circuiting goodness

	fir::IRBlock* currentBlock = cgi->builder.getCurrentBlock();

	fir::IRBlock* checkRight = cgi->builder.addNewBlockAfter("checkRight", currentBlock);
	fir::IRBlock* setTrue = cgi->builder.addNewBlockAfter("setTrue", currentBlock);
	fir::IRBlock* merge = cgi->builder.addNewBlockAfter("merge", currentBlock);


	cgi->builder.setCurrentBlock(currentBlock);

	fir::Value* resPtr = cgi->allocateInstanceInBlock(fir::PrimitiveType::getBool());
	cgi->builder.CreateStore(fir::ConstantInt::getBool(false), resPtr);

	// always codegen left.
	fir::Value* lhs = args[0]->codegen(cgi).result.first;
	if(lhs->getType() != fir::PrimitiveType::getBool())
		error(args[0], "Value of type %s cannot be implicitly casted to a boolean", lhs->getType()->str().c_str());

	// branch to either setTrue or merge.
	cgi->builder.CreateCondBranch(lhs, checkRight, merge);


	// in this block, the first condition is true.
	// so if the second condition is also true, then we can jump to merge.
	cgi->builder.setCurrentBlock(checkRight);

	fir::Value* rhs = args[1]->codegen(cgi).result.first;

	if(lhs->getType() != fir::PrimitiveType::getBool())
		error(args[1], "Value of type %s cannot be implicitly casted to a boolean", lhs->getType()->str().c_str());

	cgi->builder.CreateCondBranch(rhs, setTrue, merge);


	// this block's sole purpose is to set the thing to true.
	cgi->builder.setCurrentBlock(setTrue);

	cgi->builder.CreateStore(fir::ConstantInt::getBool(true), resPtr);
	cgi->builder.CreateUnCondBranch(merge);

	// go back to the merge.
	cgi->builder.setCurrentBlock(merge);


	return Result_t(cgi->builder.CreateLoad(resPtr), resPtr);
}

Result_t operatorLogicalOr(CodegenInstance* cgi, ArithmeticOp op, Expr* user, std::deque<Expr*> args)
{
	if(args.size() != 2)
		error(user, "Expected 2 arguments for operator %s", Parser::arithmeticOpToString(cgi, op).c_str());


	// much the same as logical and,
	// except the first branch goes to setTrue/checkRight, instead of checkRight/merge.


	fir::IRBlock* currentBlock = cgi->builder.getCurrentBlock();

	fir::IRBlock* checkRight = cgi->builder.addNewBlockAfter("checkRight", currentBlock);
	fir::IRBlock* setTrue = cgi->builder.addNewBlockAfter("setTrue", currentBlock);
	fir::IRBlock* merge = cgi->builder.addNewBlockAfter("merge", currentBlock);


	cgi->builder.setCurrentBlock(currentBlock);

	fir::Value* resPtr = cgi->allocateInstanceInBlock(fir::PrimitiveType::getBool());
	cgi->builder.CreateStore(fir::ConstantInt::getBool(false), resPtr);

	// always codegen left.
	fir::Value* lhs = args[0]->codegen(cgi).result.first;
	if(lhs->getType() != fir::PrimitiveType::getBool())
		error(args[0], "Value of type %s cannot be implicitly casted to a boolean", lhs->getType()->str().c_str());

	// branch to either setTrue or merge.
	cgi->builder.CreateCondBranch(lhs, setTrue, checkRight);


	// in this block, the first condition is true.
	// so if the second condition is also true, then we can jump to merge.
	cgi->builder.setCurrentBlock(checkRight);

	fir::Value* rhs = args[1]->codegen(cgi).result.first;

	if(lhs->getType() != fir::PrimitiveType::getBool())
		error(args[1], "Value of type %s cannot be implicitly casted to a boolean", lhs->getType()->str().c_str());

	cgi->builder.CreateCondBranch(rhs, setTrue, merge);


	// this block's sole purpose is to set the thing to true.
	cgi->builder.setCurrentBlock(setTrue);

	cgi->builder.CreateStore(fir::ConstantInt::getBool(true), resPtr);
	cgi->builder.CreateUnCondBranch(merge);


	// go back to the merge.
	cgi->builder.setCurrentBlock(merge);


	return Result_t(cgi->builder.CreateLoad(resPtr), resPtr);
}








Result_t operatorUnaryPlus(CodegenInstance* cgi, ArithmeticOp op, Expr* user, std::deque<Expr*> args)
{
	return Result_t(0, 0);
}

Result_t operatorUnaryMinus(CodegenInstance* cgi, ArithmeticOp op, Expr* user, std::deque<Expr*> args)
{
	return Result_t(0, 0);
}

Result_t operatorAddressOf(CodegenInstance* cgi, ArithmeticOp op, Expr* user, std::deque<Expr*> args)
{
	return Result_t(0, 0);
}

Result_t operatorDereference(CodegenInstance* cgi, ArithmeticOp op, Expr* user, std::deque<Expr*> args)
{
	return Result_t(0, 0);
}

Result_t operatorCustom(CodegenInstance* cgi, ArithmeticOp op, Expr* user, std::deque<Expr*> args)
{
	return Result_t(0, 0);
}






struct OperatorMap
{
	std::map<ArithmeticOp, std::function<Result_t(CodegenInstance*, ArithmeticOp, Expr*, std::deque<Expr*>)>> theMap;

	OperatorMap()
	{
		this->theMap[ArithmeticOp::Add]					= generalArithmeticOperator;
		this->theMap[ArithmeticOp::Subtract]			= generalArithmeticOperator;
		this->theMap[ArithmeticOp::Multiply]			= generalArithmeticOperator;
		this->theMap[ArithmeticOp::Divide]				= generalArithmeticOperator;
		this->theMap[ArithmeticOp::Modulo]				= generalArithmeticOperator;
		this->theMap[ArithmeticOp::ShiftLeft]			= generalArithmeticOperator;
		this->theMap[ArithmeticOp::ShiftRight]			= generalArithmeticOperator;
		this->theMap[ArithmeticOp::BitwiseAnd]			= generalArithmeticOperator;
		this->theMap[ArithmeticOp::BitwiseOr]			= generalArithmeticOperator;
		this->theMap[ArithmeticOp::BitwiseXor]			= generalArithmeticOperator;
		this->theMap[ArithmeticOp::BitwiseNot]			= generalArithmeticOperator;
		this->theMap[ArithmeticOp::CmpLT]				= generalArithmeticOperator;
		this->theMap[ArithmeticOp::CmpGT]				= generalArithmeticOperator;
		this->theMap[ArithmeticOp::CmpLEq]				= generalArithmeticOperator;
		this->theMap[ArithmeticOp::CmpGEq]				= generalArithmeticOperator;
		this->theMap[ArithmeticOp::CmpEq]				= generalArithmeticOperator;
		this->theMap[ArithmeticOp::CmpNEq]				= generalArithmeticOperator;

		this->theMap[ArithmeticOp::Assign]				= operatorAssign;
		this->theMap[ArithmeticOp::PlusEquals]			= generalCompoundAssignOperator;
		this->theMap[ArithmeticOp::MinusEquals]			= generalCompoundAssignOperator;
		this->theMap[ArithmeticOp::MultiplyEquals]		= generalCompoundAssignOperator;
		this->theMap[ArithmeticOp::DivideEquals]		= generalCompoundAssignOperator;
		this->theMap[ArithmeticOp::ModEquals]			= generalCompoundAssignOperator;
		this->theMap[ArithmeticOp::ShiftLeftEquals]		= generalCompoundAssignOperator;
		this->theMap[ArithmeticOp::ShiftRightEquals]	= generalCompoundAssignOperator;
		this->theMap[ArithmeticOp::BitwiseAndEquals]	= generalCompoundAssignOperator;
		this->theMap[ArithmeticOp::BitwiseOrEquals]		= generalCompoundAssignOperator;
		this->theMap[ArithmeticOp::BitwiseXorEquals]	= generalCompoundAssignOperator;
		this->theMap[ArithmeticOp::BitwiseNotEquals]	= generalCompoundAssignOperator;

		this->theMap[ArithmeticOp::LogicalNot]			= operatorLogicalNot;
		this->theMap[ArithmeticOp::LogicalAnd]			= operatorLogicalAnd;
		this->theMap[ArithmeticOp::LogicalOr]			= operatorLogicalOr;

		this->theMap[ArithmeticOp::Cast]				= operatorCast;
		this->theMap[ArithmeticOp::Plus]				= operatorUnaryPlus;
		this->theMap[ArithmeticOp::Minus]				= operatorUnaryMinus;
		this->theMap[ArithmeticOp::AddrOf]				= operatorAddressOf;
		this->theMap[ArithmeticOp::Deref]				= operatorDereference;
		this->theMap[ArithmeticOp::UserDefined]			= operatorCustom;
	}

	Result_t call(ArithmeticOp op, CodegenInstance* cgi, Expr* usr, std::deque<Expr*> args)
	{
		if(theMap.find(op) == theMap.end())
			op = ArithmeticOp::UserDefined;

		auto fn = theMap[op];
		return fn(cgi, op, usr, args);
	}
};

static OperatorMap operatorMap;
























































































static Result_t callOperatorOverloadOnStruct(CodegenInstance* cgi, Expr* user, ArithmeticOp op, fir::Value* lhsRef, fir::Value* rhs, fir::Value* rhsRef)
{
	if(lhsRef->getType()->getPointerElementType()->isStructType())
	{
		TypePair_t* tp = cgi->getType(lhsRef->getType()->getPointerElementType()->toStructType()->getStructName());
		if(!tp)
			return Result_t(0, 0);

		// if we can find an operator, then we call it. if not, then we'll have to handle it somewhere below.

		auto data = cgi->getOperatorOverload(user, op, lhsRef->getType()->getPointerElementType(), rhs->getType());
		Result_t ret = cgi->callOperatorOverload(data, cgi->builder.CreateLoad(lhsRef), lhsRef, rhs, rhsRef, op);

		if(ret.result.first != 0)
		{
			return ret;
		}
		else if(op != ArithmeticOp::Assign)
		{
			// only assign can conceivably be done automatically
			GenError::noOpOverload(cgi, user, reinterpret_cast<Struct*>(tp->second.first)->name, op);
		}

		// fail gracefully-ish
	}

	return Result_t(0, 0);
}







Result_t CodegenInstance::doBinOpAssign(Expr* user, Expr* left, Expr* right, ArithmeticOp op, fir::Value* lhs,
	fir::Value* ref, fir::Value* rhs, fir::Value* rhsPtr)
{
	VarRef* v		= nullptr;
	UnaryOp* uo		= nullptr;
	ArrayIndex* ari	= nullptr;

	rhs = this->autoCastType(lhs, rhs);


	// assigning something to Any
	if(this->isAnyType(lhs->getType()) || this->isAnyType(ref->getType()->getPointerElementType()))
	{
		// dealing with any.
		iceAssert(ref);
		return this->assignValueToAny(ref, rhs, rhsPtr);
	}

	// assigning Any to something
	if(rhsPtr && this->isAnyType(rhsPtr->getType()->getPointerElementType()))
	{
		// todo: find some fucking way to unwrap this shit at compile time.
		warn(left, "Unchecked assignment from 'Any' to typed variable (unfixable)");

		Result_t res = this->extractValueFromAny(lhs->getType(), rhsPtr);
		return Result_t(this->builder.CreateStore(res.result.first, ref), ref);
	}




	fir::Value* varptr = 0;
	if((v = dynamic_cast<VarRef*>(left)))
	{
		{
			VarDecl* vdecl = this->getSymDecl(user, v->name);

			if(!vdecl)
				GenError::unknownSymbol(this, user, v->name, SymbolType::Variable);

			if(vdecl->immutable)
				error(user, "Cannot assign to immutable variable '%s'!", v->name.c_str());
		}

		if(!rhs)
			error(user, "(%s:%d) -> Internal check failed: invalid RHS for assignment", __FILE__, __LINE__);

		if(SymbolPair_t* sp = this->getSymPair(user, v->name))
		{
			varptr = sp->first;
		}
		else
		{
			GenError::unknownSymbol(this, user, v->name, SymbolType::Variable);
		}



		// try and see if we have operator overloads for bo thing
		Result_t tryOpOverload = callOperatorOverloadOnStruct(this, user, op, ref, rhs, rhsPtr);
		if(tryOpOverload.result.first != 0)
			return tryOpOverload;



		lhs = this->autoCastType(rhs, lhs);
		if(!this->areEqualTypes(lhs->getType(), rhs->getType()))
		{
			// ensure we can always store 0 to pointers without a cast
			Number* n = 0;
			if(op == ArithmeticOp::Assign && rhs->getType()->isIntegerType() && (n = dynamic_cast<Number*>(right)) && n->ival == 0)
			{
				rhs = fir::ConstantValue::getNullValue(varptr->getType()->getPointerElementType());
			}
			else if(lhs->getType()->isPointerType() && rhs->getType()->isIntegerType())
			{
				// do pointer arithmetic.
				auto res = this->doPointerArithmetic(op, lhs, ref, rhs).result;
				this->builder.CreateStore(res.first, ref);

				return Result_t(res.first, res.second);
			}
			else if(lhs->getType()->isPointerType() && rhs->getType()->isArrayType()
				&& (lhs->getType()->getPointerElementType() == rhs->getType()->toArrayType()->getElementType()))
			{
				// this GEP is needed to convert the "pointer-to-array" into a "pointer-to-element"
				// ie. do what C does implicitly, where the array is really just a pointer to
				// the first element.

				// except we get nice checking with this.

				// fir::Value* r = this->builder.CreateConstGEP2_32(rhsPtr, 0, 0, "doStuff");
				fir::Value* r = this->builder.CreateGetPointer(rhsPtr, fir::ConstantInt::getUint64(0, this->getContext()));

				this->builder.CreateStore(r, ref);
				return Result_t(r, ref);
			}
			else
			{
				GenError::invalidAssignment(this, user, lhs, rhs);
			}
		}
		else if(this->isEnum(lhs->getType()))
		{
			if(this->isEnum(rhs->getType()))
			{
				fir::Value* rptr = rhsPtr;
				if(!rptr)
				{
					// fuck it, create a temporary.
					fir::Value* temp = this->allocateInstanceInBlock(rhs->getType());
					iceAssert(temp->getType()->getPointerElementType()->isStructType());

					this->builder.CreateStore(rhs, temp);
					rptr = temp;
				}

				iceAssert(rptr);
				iceAssert(this->areEqualTypes(lhs->getType(), rhs->getType()));

				// put the rhs thingy into the lhs.
				fir::Value* lgep = this->builder.CreateStructGEP(ref, 0);
				fir::Value* rgep = this->builder.CreateStructGEP(rptr, 0);

				iceAssert(lgep->getType() == rgep->getType());
				fir::Value* rval = this->builder.CreateLoad(rgep);

				this->builder.CreateStore(rval, lgep);

				return Result_t(rval, ref);
			}
			else if(TypePair_t* tp = this->getType(lhs->getType()))
			{
				if(!tp)
					error(left, "??? error!");

				if(tp->second.second != TypeKind::Enum)
					error(left, "not an enum??? you lied!");


				// todo: untested
				Enumeration* enr = dynamic_cast<Enumeration*>(tp->second.first);
				iceAssert(enr);

				if(enr->isStrong)
				{
					error(right, "Trying to assign non-enum value to an variable of type @strong enum");
				}
				else if(lhs->getType()->toStructType()->getElementN(0) != rhs->getType())
				{
					error(right, "Assigning to assign value with type %s to enumeration with base type %s",
						this->getReadableType(rhs).c_str(), this->getReadableType(lhs).c_str());
				}
				else
				{
					// okay.
					// create a wrapper value.

					fir::Value* ai = this->allocateInstanceInBlock(lhs->getType());
					fir::Value* gep = this->builder.CreateStructGEP(ai, 0);

					this->builder.CreateStore(rhs, gep);
					this->builder.CreateStore(this->builder.CreateLoad(ai), ref);

					return Result_t(lhs, ref);
				}
			}
			else
			{
				error(right, "Assignment between incomatible types lhs %s and rhs %s", this->getReadableType(lhs).c_str(),
					this->getReadableType(rhs).c_str());
			}
		}
	}
	else if((dynamic_cast<MemberAccess*>(left))
		|| ((uo = dynamic_cast<UnaryOp*>(left)) && uo->op == ArithmeticOp::Deref)
		|| (ari = dynamic_cast<ArrayIndex*>(left)))
	{
		// we know that the ptr lives in the second element
		// so, use it

		if(ari)
		{
			// check that the base is not a constant.
			Expr* leftmost = ari;
			while(auto a = dynamic_cast<ArrayIndex*>(leftmost))
			{
				leftmost = a->arr;
			}

			if(auto avr = dynamic_cast<VarRef*>(leftmost))
			{
				if(auto vd = this->getSymDecl(avr, avr->name))
				{
					if(vd->immutable)
						error(user, "Cannot assign to immutable variable %s", vd->name.c_str());
				}
			}
		}

		varptr = ref;
		iceAssert(rhs);

		// make sure the left side is a pointer
		if(!varptr)
			error(user, "Cannot assign to immutable or temporary value");

		else if(!varptr->getType()->isPointerType())
			GenError::invalidAssignment(this, user, varptr, rhs);



		// redo the number casting
		if(rhs->getType()->isIntegerType() && lhs->getType()->isIntegerType())
			rhs = this->builder.CreateIntSizeCast(rhs, varptr->getType()->getPointerElementType());

		else if(rhs->getType()->isIntegerType() && lhs->getType()->isPointerType())
			rhs = this->builder.CreateIntToPointerCast(rhs, lhs->getType());
	}
	else
	{
		error(user, "Left-hand side of assignment must be assignable (type: %s)", typeid(*left).name());
	}



	if(varptr->getType()->getPointerElementType()->isStructType())
	{
		Result_t tryOpOverload = callOperatorOverloadOnStruct(this, user, op, varptr, rhs, rhsPtr);
		if(tryOpOverload.result.first != 0)
			return tryOpOverload;
	}


	// check for overflow
	if(lhs->getType()->isIntegerType())
	{
		Number* n = 0;
		uint64_t max = 1;

		// why the fuck does c++ not have a uint64_t pow function
		{
			for(unsigned int i = 0; i < lhs->getType()->toPrimitiveType()->getIntegerBitWidth(); i++)
				max *= 2;
		}

		if(max == 0)
			max = -1;

		if((n = dynamic_cast<Number*>(right)) && !n->decimal)
		{
			bool shouldwarn = false;
			if(!this->isSignedType(n))
			{
				if((uint64_t) n->ival > max)
					shouldwarn = true;
			}
			else
			{
				max /= 2;		// minus one bit for signed types

				if(n->ival > (int64_t) max)
					shouldwarn = true;
			}

			if(shouldwarn)
			{
				warn(user, "Value '%" PRIu64 "' is too large for variable type '%s', max %lld", n->ival,
					this->getReadableType(lhs->getType()).c_str(), max);
			}
		}
	}


	// do it all together now
	if(op == ArithmeticOp::Assign)
	{
		// varptr = this->lastMinuteUnwrapType(varptr);

		this->builder.CreateStore(rhs, varptr);
		return Result_t(rhs, varptr);
	}
	else
	{
		// get the op
		fir::Value* newrhs = this->builder.CreateBinaryOp(op, lhs, rhs);
		iceAssert(newrhs);

		this->builder.CreateStore(newrhs, varptr);
		return Result_t(newrhs, varptr);
	}
}



































Result_t BinOp::codegen(CodegenInstance* cgi, fir::Value* _lhsPtr, fir::Value* _rhs)
{
	iceAssert(this->left && this->right);

	{
		auto res = operatorMap.call(this->op, cgi, this, { this->left, this->right });
		if(res.result.first)
			return res;
	}


	ValPtr_t valptr;

	fir::Value* lhs;
	fir::Value* rhs;

	if(this->op == ArithmeticOp::Assign
		|| this->op == ArithmeticOp::PlusEquals			|| this->op == ArithmeticOp::MinusEquals
		|| this->op == ArithmeticOp::MultiplyEquals		|| this->op == ArithmeticOp::DivideEquals
		|| this->op == ArithmeticOp::ModEquals			|| this->op == ArithmeticOp::ShiftLeftEquals
		|| this->op == ArithmeticOp::ShiftRightEquals	|| this->op == ArithmeticOp::BitwiseAndEquals
		|| this->op == ArithmeticOp::BitwiseOrEquals	|| this->op == ArithmeticOp::BitwiseXorEquals)
	{
		// uhh. we kinda need the rhs, but sometimes the rhs needs the lhs.
		// note: when? usually the lhs needs the rhs eg. during fake assigns (subscript/cprops)
		// todo: fix???
		// was doing lhs without rhs, then rhs with lhs *before*.
		// swapped order to fix computed property setters.

		auto res = this->right->codegen(cgi /*, valptr.second*/).result;

		valptr = this->left->codegen(cgi, 0, res.first).result;


		rhs = res.first;
		lhs = valptr.first;

		fir::Value* rhsPtr = res.second;

		rhs = cgi->autoCastType(lhs, rhs, rhsPtr);
		return cgi->doBinOpAssign(this, this->left, this->right, this->op, lhs, valptr.second, rhs, rhsPtr);
	}
	else if(this->op == ArithmeticOp::Cast || this->op == ArithmeticOp::ForcedCast)
	{
		valptr = this->left->codegen(cgi).result;
		lhs = valptr.first;

		fir::Type* rtype = cgi->getExprType(this->right);
		if(!rtype)
		{
			GenError::unknownSymbol(cgi, this, this->right->type.strType, SymbolType::Type);
		}


		iceAssert(rtype);
		if(lhs->getType() == rtype)
		{
			warn(this, "Redundant cast");
			return Result_t(lhs, 0);
		}

		if(this->op != ArithmeticOp::ForcedCast)
		{
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
					fir::Value* tmp = cgi->allocateInstanceInBlock(rtype, "tmp_enum");

					fir::Value* gep = cgi->builder.CreateStructGEP(tmp, 0);
					cgi->builder.CreateStore(lhs, gep);

					return Result_t(cgi->builder.CreateLoad(tmp), tmp);
				}
				else
				{
					error(this, "Enum '%s' does not have type '%s', invalid cast", rtype->toStructType()->getStructName().c_str(),
						lhs->getType()->str().c_str());
				}
			}
			else if(cgi->isEnum(lhs->getType()) && lhs->getType()->toStructType()->getElementN(0) == rtype)
			{
				iceAssert(valptr.second);
				fir::Value* gep = cgi->builder.CreateStructGEP(valptr.second, 0);
				fir::Value* val = cgi->builder.CreateLoad(gep);

				return Result_t(val, gep);
			}
		}



		if(cgi->isAnyType(lhs->getType()))
		{
			iceAssert(valptr.second);
			return cgi->extractValueFromAny(rtype, valptr.second);
		}
		else if(lhs->getType()->isStructType() && lhs->getType()->toStructType()->getStructName() == "String"
			&& rtype == fir::PointerType::getInt8Ptr(cgi->getContext()))
		{
			auto strPair = cgi->getType(cgi->mangleWithNamespace("String", std::deque<std::string>()));
			fir::StructType* stringType = dynamic_cast<fir::StructType*>(strPair->first);

			// string to int8*.
			// just access the data pointer.

			fir::Value* lhsref = valptr.second;
			if(!lhsref)
			{
				// dammit.
				lhsref = cgi->allocateInstanceInBlock(stringType);
				cgi->builder.CreateStore(lhs, lhsref);
			}

			fir::Value* stringPtr = cgi->builder.CreateStructGEP(lhsref, 0);
			return Result_t(cgi->builder.CreateLoad(stringPtr), stringPtr);
		}
		else if(lhs->getType() == fir::PointerType::getInt8Ptr(cgi->getContext())
			&& rtype->isStructType() && rtype->toStructType()->getStructName() == "String")
		{
			// support this shit.
			// error(cgi, this, "Automatic char* -> String casting not yet supported");

			// create a bogus func call.
			TypePair_t* tp = cgi->getType("String");
			iceAssert(tp);

			std::vector<fir::Value*> args { lhs };
			return cgi->callTypeInitialiser(tp, this, args);
		}
		else if(this->op != ArithmeticOp::ForcedCast)
		{
			std::string lstr = cgi->getReadableType(lhs).c_str();
			std::string rstr = cgi->getReadableType(rtype).c_str();

			// if(!fir::CastInst::castIsValid(fir::Instruction::BitCast, lhs, rtype))
			// {
			// 	error(this, "Invalid cast from type %s to %s", lstr.c_str(), rstr.c_str());
			// }
			// else

			warn(this, "Unknown cast, doing raw bitcast (from type %s to %s)", lstr.c_str(), rstr.c_str());
		}

		return Result_t(cgi->builder.CreateBitcast(lhs, rtype), 0);
	}
	else
	{
		valptr = this->left->codegen(cgi).result;
	}







	// else case.
	// no point being explicit about this and wasting indentation

	lhs = valptr.first;
	fir::Value* lhsPtr = valptr.second;
	fir::Value* lhsptr = valptr.second;
	auto r = this->right->codegen(cgi).result;

	rhs = r.first;
	fir::Value* rhsPtr = r.second;
	rhs = cgi->autoCastType(lhs, rhs, r.second);

	iceAssert(lhs);
	iceAssert(rhs);



	bool isBuiltinIntegerOp = false;
	{
		bool lhsInteger = false;
		bool rhsInteger = false;

		if(lhs->getType()->isIntegerType())
		{
			lhsInteger = true;
		}

		if(rhs->getType()->isIntegerType())
		{
			rhsInteger = true;

		}

		isBuiltinIntegerOp = (lhsInteger && rhsInteger);
	}
















	if(lhs->getType()->isPointerType() && rhs->getType()->isIntegerType())
	{
		if((this->op == ArithmeticOp::Add || this->op == ArithmeticOp::Subtract || this->op == ArithmeticOp::PlusEquals
			|| this->op == ArithmeticOp::MinusEquals))
		{
			return cgi->doPointerArithmetic(this->op, lhs, lhsptr, rhs);
		}
		else if(this->op == ArithmeticOp::CmpEq || this->op == ArithmeticOp::CmpNEq)
		{
			Number* n = dynamic_cast<Number*>(this->right);
			if(n && !n->decimal && n->ival == 0)
			{
				fir::Value* casted = cgi->builder.CreatePointerToIntCast(lhs, rhs->getType());

				if(this->op == ArithmeticOp::CmpEq)
					return Result_t(cgi->builder.CreateICmpEQ(casted, rhs), 0);
				else
					return Result_t(cgi->builder.CreateICmpNEQ(casted, rhs), 0);
			}
		}
	}
	else if(isBuiltinIntegerOp)
	{
		rhs = cgi->autoCastType(lhs, rhs);

		fir::Value* tryop = cgi->builder.CreateBinaryOp(this->op, lhs, rhs);
		if(tryop)
		{
			return Result_t(tryop, 0);
		}

		fir::Value* ret = 0;
		switch(this->op)
		{
			// comparisons
			case ArithmeticOp::CmpEq:
				ret = cgi->builder.CreateICmpEQ(lhs, rhs);
				break;

			case ArithmeticOp::CmpNEq:
				ret = cgi->builder.CreateICmpNEQ(lhs, rhs);
				break;

			case ArithmeticOp::CmpLT:
				ret = cgi->builder.CreateICmpLT(lhs, rhs);
				break;

			case ArithmeticOp::CmpGT:
				ret = cgi->builder.CreateICmpGT(lhs, rhs);
				break;

			case ArithmeticOp::CmpLEq:
				ret = cgi->builder.CreateICmpLEQ(lhs, rhs);
				break;

			case ArithmeticOp::CmpGEq:
				ret = cgi->builder.CreateICmpGEQ(lhs, rhs);
				break;

			default:
				// should not be reached
				error("what?!");
		}

		ret = cgi->builder.CreateIntSizeCast(ret, fir::PrimitiveType::getBool(cgi->getContext()));
		return Result_t(ret, 0);
	}
	else if(cgi->isBuiltinType(this->left) && cgi->isBuiltinType(this->right))
	{
		// if one of them is an integer, cast it first
		rhs = cgi->autoCastType(lhs, rhs, r.second);

		if(lhs->getType() != rhs->getType())
		{
			error(this, "Left and right-hand side of binary expression do not have have the same type! (%s vs %s)",
				cgi->getReadableType(lhs).c_str(), cgi->getReadableType(rhs).c_str());
		}

		// then they're floats.
		switch(this->op)
		{
			case ArithmeticOp::Add:			return Result_t(cgi->builder.CreateAdd(lhs, rhs), 0);
			case ArithmeticOp::Subtract:	return Result_t(cgi->builder.CreateSub(lhs, rhs), 0);
			case ArithmeticOp::Multiply:	return Result_t(cgi->builder.CreateMul(lhs, rhs), 0);
			case ArithmeticOp::Divide:		return Result_t(cgi->builder.CreateDiv(lhs, rhs), 0);

			// comparisons
			case ArithmeticOp::CmpEq:		return Result_t(cgi->builder.CreateFCmpEQ_ORD(lhs, rhs), 0);
			case ArithmeticOp::CmpNEq:		return Result_t(cgi->builder.CreateFCmpNEQ_ORD(lhs, rhs), 0);
			case ArithmeticOp::CmpLT:		return Result_t(cgi->builder.CreateFCmpLT_ORD(lhs, rhs), 0);
			case ArithmeticOp::CmpGT:		return Result_t(cgi->builder.CreateFCmpGT_ORD(lhs, rhs), 0);
			case ArithmeticOp::CmpLEq:		return Result_t(cgi->builder.CreateFCmpLEQ_ORD(lhs, rhs), 0);
			case ArithmeticOp::CmpGEq:		return Result_t(cgi->builder.CreateFCmpGEQ_ORD(lhs, rhs), 0);

			default:						error(this, "Unsupported operator. (%d)", this->op);
		}
	}
	else if(cgi->isEnum(lhs->getType()) && cgi->isEnum(rhs->getType()))
	{
		iceAssert(lhsPtr);
		iceAssert(rhsPtr);

		fir::Value* gepL = cgi->builder.CreateStructGEP(lhsPtr, 0);
		fir::Value* gepR = cgi->builder.CreateStructGEP(rhsPtr, 0);

		fir::Value* l = cgi->builder.CreateLoad(gepL);
		fir::Value* r = cgi->builder.CreateLoad(gepR);

		fir::Value* res = 0;

		if(this->op == ArithmeticOp::CmpEq)
		{
			res = cgi->builder.CreateICmpEQ(l, r);
		}
		else if(this->op == ArithmeticOp::CmpNEq)
		{
			res = cgi->builder.CreateICmpNEQ(l, r);
		}
		else
		{
			error(this, "Invalid comparison %s on Type!", Parser::arithmeticOpToString(cgi, this->op).c_str());
		}


		return Result_t(res, 0);
	}
	else if(lhs->getType()->isStructType() || rhs->getType()->isStructType())
	{
		auto data = cgi->getOperatorOverload(this, op, valptr.first->getType(), rhs->getType());
		Result_t ret = cgi->callOperatorOverload(data, valptr.first, valptr.second, rhs, r.second, op);

		if(ret.result.first == 0)
		{
			error(this, "No such operator '%s' for expression %s %s %s", Parser::arithmeticOpToString(cgi, op).c_str(),
				cgi->getReadableType(lhs).c_str(), Parser::arithmeticOpToString(cgi, op).c_str(), cgi->getReadableType(rhs).c_str());
		}

		return ret;
	}

	error(this, "Unsupported operator '%s' on types %s and %s", Parser::arithmeticOpToString(cgi, op).c_str(),
		lhs->getType()->str().c_str(), rhs->getType()->str().c_str());
}





