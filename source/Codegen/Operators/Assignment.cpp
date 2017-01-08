// Assignment.cpp
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
	static fir::Function* tryGetComputedPropSetter(CodegenInstance* cgi, MemberAccess* ma)
	{
		VarRef* vrname = 0;
		if(!(vrname = dynamic_cast<VarRef*>(ma->right)))
			return 0;

		fir::Type* leftType = (ma->matype == MAType::LeftVariable ? ma->left->getType(cgi) : cgi->resolveStaticDotOperator(ma, false).second);

		if(leftType->isPrimitiveType() && cgi->getExtensionsForBuiltinType(leftType).size() > 0)
		{
			// great, just great.
			auto exts = cgi->getExtensionsForBuiltinType(leftType);

			ComputedProperty* prop = 0;
			for(auto ext : exts)
			{
				for(auto c : ext->cprops)
				{
					if(c->ident.name == vrname->name)
					{
						prop = c;
						goto out;
					}
				}
			}

			out:
			if(prop == 0) return 0;
			if(!prop->setterFunc) return 0;

			// assert here, because it should be had.
			iceAssert(prop->setterFFn);

			return cgi->module->getOrCreateFunction(prop->setterFFn->getName(), prop->setterFFn->getType(), prop->setterFFn->linkageType);
		}
		else if(!leftType || (!leftType->isStructType() && (leftType->isPointerType() && !leftType->getPointerElementType()->isStructType())
							&& !leftType->isClassType() && (leftType->isPointerType() && !leftType->getPointerElementType()->isClassType())))
		{
			return 0;
		}

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

		if(!ret)
		{
			for(auto ext : cgi->getExtensionsForType(cls))
			{
				for(auto cp : ext->cprops)
				{
					if(cp->ident.name == vrname->name)
					{
						// found
						ret = cp;
						break;
					}
				}
			}
		}

		if(!ret) return 0;
		if(!ret->setterFunc) return 0;

		// assert here, because it should be had.
		iceAssert(ret->setterFFn);

		return cgi->module->getFunction(ret->setterFFn->getName());
	}


	Result_t operatorAssign(CodegenInstance* cgi, ArithmeticOp op, Expr* user, std::deque<Expr*> args)
	{
		if(args.size() != 2)
			error(user, "Expected 2 arguments for operator %s", Parser::arithmeticOpToString(cgi, op).c_str());

		ValueKind vk = ValueKind::RValue;
		fir::Value* rhsptr = 0;
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
			std::tie(rhs, rhsptr, vk) = OperatorMap::get().call(actualOp, cgi, user, args);
			iceAssert(rhs);

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

				auto lres = ma->left->codegen(cgi);
				fir::Value* lhsPtr = lres.value->getType()->isPointerType() ? lres.value : lres.pointer;
				iceAssert(lhsPtr);
				if(lhsPtr->isImmutable())
					GenError::assignToImmutable(cgi, user, args[1]);

				fir::Value* rhsVal = rhs ? rhs : args[1]->codegen(cgi, lhsPtr).value;

				cgi->irb.CreateCall2(setter, lhsPtr, rhsVal);

				return Result_t(0, 0);
			}
		}
		else if(ArrayIndex* ai = dynamic_cast<ArrayIndex*>(args[0]))
		{
			// also check if the left side is a subscript on a type.
			fir::Type* t = ai->arr->getType(cgi);

			if(t->isStringType())
			{
				// ok.
				// do some stuff.
				// check if the string is a literal

				// requires runtime code check
				auto leftr = ai->arr->codegen(cgi);
				iceAssert(leftr.value);

				fir::Value* ind = ai->index->codegen(cgi).value;

				if(!ind->getType()->isIntegerType())
					error(ai->index, "Subscript index must be an integer type, got '%s'", ind->getType()->str().c_str());

				cgi->irb.CreateCall2(RuntimeFuncs::String::getCheckLiteralWriteFunction(cgi), leftr.value, ind);
				cgi->irb.CreateCall2(RuntimeFuncs::String::getBoundsCheckFunction(cgi), leftr.value, ind);

				fir::Value* dp = cgi->irb.CreateGetStringData(leftr.value);
				fir::Value* ptr = cgi->irb.CreateGetPointer(dp, ind);

				fir::Value* val = args[1]->codegen(cgi).value;

				if(!val->getType()->isCharType())
					error(args[1], "Assigning incompatible type '%s' to subscript of string", val->getType()->str().c_str());

				val = cgi->irb.CreateBitcast(val, fir::Type::getInt8());

				cgi->irb.CreateStore(val, ptr);
				return Result_t(0, 0);
			}
			else if(!t->isPointerType() && !t->isArrayType() && !t->isParameterPackType() && !t->isDynamicArrayType())
			{
				return operatorAssignToOverloadedSubscript(cgi, op, user, args[0], rhs ? rhs : args[1]->codegen(cgi).value, args[1]);
			}
		}




		// else, we should be safe to codegen both sides
		fir::Value* lhs = 0;
		fir::Value* lhsptr = 0;

		std::tie(lhs, lhsptr) = args[0]->codegen(cgi);


		// this is to allow handling of compound assignment operators
		// if we are one, then the rhs will already have been generated, and we can't do codegen (again).
		if(rhs == 0 && rhsptr == 0)
		{
			iceAssert(rhs == 0);
			iceAssert(rhsptr == 0);

			std::tie(rhs, rhsptr, vk) = args[1]->codegen(cgi, lhsptr);
		}


		// the bulk of the work is still done here
		return performActualAssignment(cgi, user, args[0], args[1], op, lhs, lhsptr, rhs, rhsptr, vk);
	}





	Result_t generalCompoundAssignOperator(CodegenInstance* cgi, ArithmeticOp op, Expr* user, std::deque<Expr*> args)
	{
		if(args.size() != 2)
			error(user, "Expected 2 arguments for operator %s", Parser::arithmeticOpToString(cgi, op).c_str());


		fir::Type* ltype = args[0]->getType(cgi);
		fir::Type* rtype = args[1]->getType(cgi, false, fir::ConstantValue::getNullValue(ltype->getPointerTo()));

		if(ltype->isStructType() || rtype->isStructType() || ltype->isClassType() || rtype->isClassType())
		{
			// first check if we have an overload for the compound thing as a whole.
			auto data = cgi->getBinaryOperatorOverload(user, op, ltype, rtype);
			if(data.found)
			{
				fir::Value* lhs = 0; fir::Value* lhsptr = 0;
				fir::Value* rhs = 0; fir::Value* rhsptr = 0;

				std::tie(lhs, lhsptr) = args[0]->codegen(cgi);
				std::tie(rhs, rhsptr) = args[1]->codegen(cgi);

				cgi->callBinaryOperatorOverload(data, lhs, lhsptr, rhs, rhsptr, op);
				return Result_t(0, 0);
			}
		}
		// special case: array += array, array += element
		else if(op == ArithmeticOp::PlusEquals && ltype->isDynamicArrayType() && rtype->isDynamicArrayType()
			&& ltype->toDynamicArrayType()->getElementType() == rtype->toDynamicArrayType()->getElementType())
		{
			// array += array
			fir::Value* lhs = 0; fir::Value* lhsptr = 0;
			fir::Value* rhs = 0; fir::Value* rhsptr = 0;

			std::tie(lhs, lhsptr) = args[0]->codegen(cgi);
			std::tie(rhs, rhsptr) = args[1]->codegen(cgi);

			iceAssert(lhs->getType()->isDynamicArrayType());
			fir::DynamicArrayType* arrtype = lhs->getType()->toDynamicArrayType();

			iceAssert(lhsptr);

			// we can always do var += rvalue, so we need to make an rhsptr
			if(!rhsptr)
				rhsptr = cgi->irb.CreateImmutStackAlloc(rhs->getType(), rhs);


			if(lhsptr->isImmutable())
				GenError::assignToImmutable(cgi, user, args[0]);

			// ok, call append.
			fir::Function* appendf = RuntimeFuncs::Array::getAppendFunction(cgi, arrtype);
			iceAssert(appendf);

			cgi->irb.CreateCall2(appendf, lhsptr, rhsptr);

			// return void
			return Result_t(0, 0);
		}
		else if(op == ArithmeticOp::PlusEquals && ltype->isDynamicArrayType()
			&& ltype->toDynamicArrayType()->getElementType() == rtype)
		{
			// array += element
			fir::Value* lhs = 0; fir::Value* lhsptr = 0;
			fir::Value* rhs = 0; fir::Value* rhsptr = 0;

			ValueKind rhsvk;
			std::tie(lhs, lhsptr) = args[0]->codegen(cgi);
			std::tie(rhs, rhsptr, rhsvk) = args[1]->codegen(cgi);

			iceAssert(lhs->getType()->isDynamicArrayType());
			fir::DynamicArrayType* arrtype = lhs->getType()->toDynamicArrayType();

			iceAssert(lhsptr);
			iceAssert(rhs);

			if(lhsptr->isImmutable())
				GenError::assignToImmutable(cgi, user, args[0]);

			// ok, call append.
			fir::Function* appendf = RuntimeFuncs::Array::getElementAppendFunction(cgi, arrtype);
			iceAssert(appendf);

			cgi->irb.CreateCall2(appendf, lhsptr, rhs);


			// handle some shit
			if(cgi->isRefCountedType(rtype))
				cgi->removeRefCountedValueIfExists(rhsptr);


			// return void
			return Result_t(0, 0);
		}

		// else, we'll rely on + and = separation/synthesis.
		return operatorAssign(cgi, op, user, args);
	}





	Result_t performActualAssignment(CodegenInstance* cgi, Expr* user, Expr* leftExpr, Expr* rightExpr, ArithmeticOp op, fir::Value* lhs,
		fir::Value* lhsPtr, fir::Value* rhs, fir::Value* rhsPtr, ValueKind vk)
	{
		// check whether the left side is a struct, and if so do an operator overload call
		iceAssert(op == ArithmeticOp::Assign);


		if(lhsPtr && lhsPtr->isImmutable())
		{
			GenError::assignToImmutable(cgi, user, leftExpr);
		}

		if(lhs == 0)
		{
			GenError::nullValue(cgi, user);
		}





		// check function assign
		if(lhs->getType()->isFunctionType() && rhs->getType()->isFunctionType())
		{
			// rhs is a generic function, we need to concretise the left side.
			// the left side can't be generic, because that doesn't make sense.
			iceAssert(!lhs->getType()->toFunctionType()->isGenericFunction());

			if(rhs->getType()->toFunctionType()->isGenericFunction())
			{
				fir::Function* oldf = dynamic_cast<fir::Function*>(rhs);

				// oldf can be null.
				fir::Function* res = cgi->instantiateGenericFunctionUsingValueAndType(rightExpr, oldf, rhs->getType()->toFunctionType(),
					lhs->getType()->toFunctionType(), dynamic_cast<MemberAccess*>(rightExpr));

				iceAssert(res);

				// rewrite history
				rhs = res;
			}
		}



		if((lhs->getType()->isStructType() || lhs->getType()->isClassType()) && !cgi->isAnyType(lhs->getType()))
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
				// for structs, we just assign the members.
				cgi->irb.CreateStore(rhs, lhsPtr);
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

			cgi->irb.CreateStore(res.value, lhsPtr);

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
		{
			error(user, "Invalid assignment from value of type %s to one of type %s", lhs->getType()->str().c_str(),
				rhs->getType()->str().c_str());
		}





		if(cgi->isRefCountedType(lhs->getType()))
		{
			cgi->assignRefCountedExpression(user, rhs, rhsPtr, lhs, lhsPtr, vk, false, true);
		}
		else if(VarRef* v = dynamic_cast<VarRef*>(leftExpr))
		{
			VarDecl* vdecl = cgi->getSymDecl(user, v->name);

			if(!vdecl)
				GenError::unknownSymbol(cgi, user, v->name, SymbolType::Variable);

			if(vdecl->immutable || lhsPtr->isImmutable())
				error(user, "Cannot assign to immutable variable '%s'!", v->name.c_str());

			// store it, and return 0.
			cgi->irb.CreateStore(rhs, lhsPtr);
		}
		else
		{
			// just do it
			iceAssert(rhs);
			iceAssert(lhsPtr);

			cgi->irb.CreateStore(rhs, lhsPtr);
		}

		return Result_t(0, 0);
	}
}
















