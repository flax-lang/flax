// Assignment.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "codegen.h"
#include "operators.h"

using namespace Ast;
using namespace Codegen;

namespace Operators
{
	static fir::Function* tryGetComputedPropSetter(CodegenInstance* cgi, MemberAccess* ma)
	{
		VarRef* vrname = 0;
		if(!(vrname = dynamic_cast<VarRef*>(ma->right)))
			return 0;

		fir::Type* leftType = (ma->matype == MAType::LeftVariable ? cgi->getExprType(ma->left) : cgi->resolveStaticDotOperator(ma, false).second);
		if(!leftType || (!leftType->isStructType() && (leftType->isPointerType() && !leftType->getPointerElementType()->isStructType())))
			return 0;

		TypePair_t* tp = cgi->getType(leftType);
		if(!tp && leftType->isPointerType()) { tp = cgi->getType(leftType->getPointerElementType()); }
		if(!tp)
			return 0;

		ClassDef* cls = dynamic_cast<ClassDef*>(tp->second.first);
		if(!cls)
			return 0;

		ComputedProperty* ret = 0;
		for(auto cp : cls->cprops)
		{
			if(cp->ident.name == vrname->name)
			{
				// found
				ret = cp;
				break;
			}
		}

		if(!ret) return 0;
		if(!ret->setterFunc) return 0;

		// assert here, because it should be had.
		iceAssert(ret->setterFFn);

		return ret->setterFFn;
	}


	Result_t operatorAssign(CodegenInstance* cgi, ArithmeticOp op, Expr* user, std::deque<Expr*> args)
	{
		if(args.size() != 2)
			error(user, "Expected 2 arguments for operator %s", Parser::arithmeticOpToString(cgi, op).c_str());


		fir::Value* rhsPtr = 0;
		fir::Value* rhs = 0;


		if(op != ArithmeticOp::Assign)
		{
			ArithmeticOp actualOp;
			switch(op)
			{
				case ArithmeticOp::PlusEquals:			actualOp = ArithmeticOp::Add; break;
				case ArithmeticOp::MinusEquals:			actualOp = ArithmeticOp::Subtract; break;
				case ArithmeticOp::MultiplyEquals:		actualOp = ArithmeticOp::Multiply; break;
				case ArithmeticOp::DivideEquals:		actualOp = ArithmeticOp::Divide; break;
				case ArithmeticOp::ModEquals:			actualOp = ArithmeticOp::Modulo; break;
				case ArithmeticOp::ShiftLeftEquals:		actualOp = ArithmeticOp::ShiftLeft; break;
				case ArithmeticOp::ShiftRightEquals:	actualOp = ArithmeticOp::ShiftRight; break;
				case ArithmeticOp::BitwiseAndEquals:	actualOp = ArithmeticOp::BitwiseAnd; break;
				case ArithmeticOp::BitwiseOrEquals:		actualOp = ArithmeticOp::BitwiseOr; break;
				case ArithmeticOp::BitwiseXorEquals:	actualOp = ArithmeticOp::BitwiseXor; break;
				default:	error("what");
			}

			// note: when we reach this, it means that we didn't find a specific operator overload
			// in the "generalCompoundAssignmentOperator" function.
			Result_t res = OperatorMap::get().call(actualOp, cgi, user, args);
			iceAssert(res.result.first);

			rhs = res.result.first;
			rhsPtr = res.result.second;

			op = ArithmeticOp::Assign;
		}



		// check if it's a computed property.
		if(MemberAccess* ma = dynamic_cast<MemberAccess*>(args[0]))
		{
			// todo: move this out.
			fir::Function* setter = tryGetComputedPropSetter(cgi, ma);
			if(setter)
			{
				iceAssert(setter->getArgumentCount() == 2 && "invalid setter");

				fir::Value* rhsVal = rhs ? rhs : args[1]->codegen(cgi).result.first;

				auto lres = ma->left->codegen(cgi).result;
				fir::Value* lhsPtr = lres.first->getType()->isPointerType() ? lres.first : lres.second;

				iceAssert(lhsPtr);
				if(lhsPtr->isImmutable())
					GenError::assignToImmutable(cgi, user, args[1]);

				cgi->builder.CreateCall2(setter, lhsPtr, rhsVal);

				return Result_t(0, 0);
			}
		}
		else if(ArrayIndex* ai = dynamic_cast<ArrayIndex*>(args[0]))
		{
			// also check if the left side is a subscript on a type.
			fir::Type* t = cgi->getExprType(ai->arr);

			// todo: do we need to add the LLVariableArray thing?
			if(!t->isPointerType() && !t->isArrayType() && !t->isLLVariableArrayType())
				return operatorAssignToOverloadedSubscript(cgi, op, user, args[0], rhs ? rhs : args[1]->codegen(cgi).result.first, args[1]);
		}




		// else, we should be safe to codegen both sides
		auto leftVP = args[0]->codegen(cgi).result;
		fir::Value* lhsPtr = leftVP.second;
		fir::Value* lhs = leftVP.first;


		// this is to allow handling of compound assignment operators
		// if we are one, then the rhs will already have been generated, and we can't do codegen (again).
		if(rhs == 0 && rhsPtr == 0)
		{
			iceAssert(rhs == 0);
			iceAssert(rhsPtr == 0);

			auto rightVP = args[1]->codegen(cgi).result;
			rhsPtr = rightVP.second;
			rhs = rightVP.first;
		}


		// the bulk of the work is still done here
		return performActualAssignment(cgi, user, args[0], args[1], op, lhs, lhsPtr, rhs, rhsPtr);
	}




	Result_t generalCompoundAssignOperator(CodegenInstance* cgi, ArithmeticOp op, Expr* user, std::deque<Expr*> args)
	{
		if(args.size() != 2)
			error(user, "Expected 2 arguments for operator %s", Parser::arithmeticOpToString(cgi, op).c_str());


		fir::Type* ltype = cgi->getExprType(args[0]);
		fir::Type* rtype = cgi->getExprType(args[1]);

		if(ltype->isStructType() || rtype->isStructType())
		{
			// first check if we have an overload for the compound thing as a whole.
			auto data = cgi->getBinaryOperatorOverload(user, op, ltype, rtype);
			if(data.found)
			{
				auto leftvp = args[0]->codegen(cgi).result;
				auto rightvp = args[1]->codegen(cgi).result;

				cgi->callBinaryOperatorOverload(data, leftvp.first, leftvp.second, rightvp.first, rightvp.second, op);
				return Result_t(0, 0);
			}

			// if not, then we'll rely on + and = separation/synthesis.
		}

		// else
		return operatorAssign(cgi, op, user, args);
	}





	Result_t performActualAssignment(CodegenInstance* cgi, Expr* user, Expr* leftExpr, Expr* rightExpr, ArithmeticOp op, fir::Value* lhs,
		fir::Value* lhsPtr, fir::Value* rhs, fir::Value* rhsPtr)
	{
		// check whether the left side is a struct, and if so do an operator overload call
		iceAssert(op == ArithmeticOp::Assign);

		if(lhsPtr && lhsPtr->isImmutable())
		{
			GenError::assignToImmutable(cgi, user, leftExpr);
		}


		if(lhs->getType()->isStructType())
		{
			TypePair_t* tp = cgi->getType(lhs->getType());
			iceAssert(tp);

			if(tp->second.second == TypeKind::Class)
			{
				auto data = cgi->getBinaryOperatorOverload(user, op, lhs->getType(), rhs->getType());
				if(data.found)
				{
					fir::Function* opf = data.opFunc;
					iceAssert(opf);
					iceAssert(opf->getArgumentCount() == 2);
					iceAssert(opf->getArguments()[0]->getType() == lhs->getType()->getPointerTo());
					iceAssert(opf->getArguments()[1]->getType() == rhs->getType());

					iceAssert(lhsPtr);

					cgi->callBinaryOperatorOverload(data, lhs, lhsPtr, rhs, rhsPtr, op);

					return Result_t(0, 0);
				}
				else
				{
					error(user, "No valid operator overload to assign a value of type %s to one of %s", rhs->getType()->str().c_str(),
						lhs->getType()->str().c_str());
				}
			}
			else if(tp->second.second == TypeKind::Struct)
			{
				// for structs, we just assgin the members.
				cgi->builder.CreateStore(rhs, lhsPtr);
				return Result_t(0, 0);
			}
			else
			{
				error(user, "wtf? %s", lhs->getType()->str().c_str());
			}
		}



		// assigning something to Any
		if(cgi->isAnyType(lhs->getType()))
		{
			if(!rhsPtr && !rhs->getType()->isPrimitiveType() && !rhs->getType()->isPointerType())
			{
				// we need a pointer, since bytes and all, for Any.
				rhsPtr = cgi->getImmutStackAllocValue(rhs);
			}

			iceAssert(lhsPtr);
			cgi->assignValueToAny(lhsPtr, rhs, rhsPtr);

			// assign returns nothing
			return Result_t(0, 0);
		}

		// assigning Any to something
		if(cgi->isAnyType(rhs->getType()))
		{
			// todo: find some fucking way to unwrap this shit at compile time.
			warn(user, "Unchecked assignment from 'Any' to typed variable (unfixable)");

			iceAssert(rhsPtr);
			Result_t res = cgi->extractValueFromAny(lhs->getType(), rhsPtr);

			cgi->builder.CreateStore(res.result.first, lhsPtr);

			// assign returns nothing.
			return Result_t(0, 0);
		}

		if(!lhsPtr)
		{
			error(user, "Unassignable?");
		}

		// do the casting.
		if(lhsPtr->getType()->getPointerElementType() != rhs->getType())
		{
			rhs = cgi->autoCastType(lhsPtr->getType()->getPointerElementType(), rhs);
		}



		if(lhs->getType() != rhs->getType())
			error(user, "Invalid assignment from value of type %s to one of type %s", lhs->getType()->str().c_str(), rhs->getType()->str().c_str());


		// check if the left side is a simple var
		if(VarRef* v = dynamic_cast<VarRef*>(leftExpr))
		{
			VarDecl* vdecl = cgi->getSymDecl(user, v->name);

			if(!vdecl)
				GenError::unknownSymbol(cgi, user, v->name, SymbolType::Variable);

			if(vdecl->immutable || lhsPtr->isImmutable())
				error(user, "Cannot assign to immutable variable '%s'!", v->name.c_str());

			// store it, and return 0.
			cgi->builder.CreateStore(rhs, lhsPtr);
			return Result_t(0, 0);
		}
		else if(cgi->isEnum(lhs->getType()) && cgi->isEnum(rhs->getType()))
		{
			iceAssert(lhs->getType() == rhs->getType());

			// directly store the enum innards into the lhs.

			iceAssert(lhsPtr);
			iceAssert(rhsPtr);

			fir::Value* lhsGEP = cgi->builder.CreateStructGEP(lhsPtr, 0);
			fir::Value* rhsGEP = cgi->builder.CreateStructGEP(rhsPtr, 0);

			fir::Value* rhsVal = cgi->builder.CreateLoad(rhsGEP);
			cgi->builder.CreateStore(rhsVal, lhsGEP);

			return Result_t(0, 0);
		}
		else
		{
			// just do it
			iceAssert(rhs);
			iceAssert(lhsPtr);

			cgi->builder.CreateStore(rhs, lhsPtr);

			return Result_t(0, 0);
		}
	}
}
















