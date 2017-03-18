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


		// check if the left is a varref
		fir::Type* leftType = 0;
		if(VarRef* vr = dynamic_cast<VarRef*>(ma->left))
		{
			// check if it's a variable at all.
			// yes -- ok good.
			// no -- break early.
			if(auto v = cgi->getSymInst(ma->left, vr->name))
				leftType = v->getType()->getPointerElementType();

			else
				return 0;
		}
		else if(MemberAccess* lma = dynamic_cast<MemberAccess*>(ma->left))
		{
			// using variant = mpark::variant<fir::Type*, FunctionTree*, TypePair_t, Result_t>;
			auto variant = cgi->resolveTypeOfMA(lma, 0, false);
			if(variant.index() == 0)
				leftType = mpark::get<fir::Type*>(variant);

			else if(variant.index() == 2)
				leftType = mpark::get<TypePair_t>(variant).first;

			else
				return 0;
		}
		else
		{
			// no chance.
			return 0;
		}


		iceAssert(leftType);

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


	Result_t operatorAssign(CodegenInstance* cgi, ArithmeticOp op, Expr* user, std::vector<Expr*> args)
	{
		if(args.size() != 2)
			error(user, "Expected 2 arguments for operator %s", Parser::arithmeticOpToString(cgi, op).c_str());

		ValueKind rhsvk = ValueKind::RValue;
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
			std::tie(rhs, rhsptr, rhsvk) = OperatorMap::get().call(actualOp, cgi, user, args);
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

				auto loc = fir::ConstantString::get(Parser::pinToString(ai->pin));
				cgi->irb.CreateCall3(RuntimeFuncs::String::getCheckLiteralWriteFunction(cgi), leftr.value, ind, loc);
				cgi->irb.CreateCall3(RuntimeFuncs::String::getBoundsCheckFunction(cgi), leftr.value, ind, loc);

				fir::Value* dp = cgi->irb.CreateGetStringData(leftr.value);
				fir::Value* ptr = cgi->irb.CreateGetPointer(dp, ind);

				fir::Value* val = args[1]->codegen(cgi).value;

				if(!val->getType()->isCharType())
					error(args[1], "Assigning incompatible type '%s' to subscript of string", val->getType()->str().c_str());

				val = cgi->irb.CreateBitcast(val, fir::Type::getInt8());

				cgi->irb.CreateStore(val, ptr);
				return Result_t(0, 0);
			}
			else if(!t->isPointerType() && !t->isArrayType() && !t->isDynamicArrayType() && !t->isArraySliceType())
			{
				return operatorAssignToOverloadedSubscript(cgi, op, user, args[0], rhs ? rhs : args[1]->codegen(cgi).value, args[1]);
			}
		}




		// else, we should be safe to codegen both sides
		fir::Value* lhs = 0;
		fir::Value* lhsptr = 0;
		ValueKind lhsvk = ValueKind::RValue;

		std::tie(lhs, lhsptr, lhsvk) = args[0]->codegen(cgi);


		// this is to allow handling of compound assignment operators
		// if we are one, then the rhs will already have been generated, and we can't do codegen (again).
		if(rhs == 0 && rhsptr == 0)
		{
			iceAssert(rhs == 0);
			iceAssert(rhsptr == 0);

			std::tie(rhs, rhsptr, rhsvk) = args[1]->codegen(cgi, lhs->getType(), lhsptr);
		}


		// the bulk of the work is still done here
		return performActualAssignment(cgi, user, args[0], args[1], op, lhs, lhsptr, lhsvk, rhs, rhsptr, rhsvk);
	}





	Result_t generalCompoundAssignOperator(CodegenInstance* cgi, ArithmeticOp op, Expr* user, std::vector<Expr*> args)
	{
		if(args.size() != 2)
			error(user, "Expected 2 arguments for operator %s", Parser::arithmeticOpToString(cgi, op).c_str());


		fir::Type* ltype = args[0]->getType(cgi);
		fir::Type* rtype = args[1]->getType(cgi, ltype);

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
		fir::Value* lhsPtr, ValueKind lhsvk, fir::Value* rhs, fir::Value* rhsPtr, ValueKind rhsvk, bool isInitial)
	{
		// check whether the left side is a struct, and if so do an operator overload call
		iceAssert(op == ArithmeticOp::Assign);


		if(lhsPtr && lhsPtr->isImmutable())
		{
			GenError::assignToImmutable(cgi, user, leftExpr);
		}

		if(lhs == 0 || lhs->getType()->isVoidType())
		{
			GenError::nullValue(cgi, user);
		}

		if(lhsvk != ValueKind::LValue || !lhsPtr)
		{
			error(leftExpr, "Invalid assignment to rvalue");
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



		if((lhs->getType()->isStructType() || lhs->getType()->isClassType()) && !lhs->getType()->isAnyType())
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
					// note/todo: perhaps this is not what we want?
					// for now, do the same thing as structs if we don't have an operator = overload
					cgi->irb.CreateStore(rhs, lhsPtr);
					return Result_t(0, 0);

					// error(user, "No valid operator overload to assign a value of type %s to one of %s", rhs->getType()->str().c_str(),
					// 	lhs->getType()->str().c_str());
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
		if(lhs->getType()->isAnyType() && !rhs->getType()->isAnyType())
		{
			if(!rhsPtr && !rhs->getType()->isPrimitiveType() && !rhs->getType()->isPointerType())
			{
				// we need a pointer, since bytes and all, for Any.
				rhsPtr = cgi->getImmutStackAllocValue(rhs);
			}

			iceAssert(lhsPtr);
			cgi->removeRefCountedValueIfExists(lhsPtr);
			cgi->assignValueToAny(user, lhsPtr, rhs, rhsPtr, rhsvk);

			// assign returns nothing
			return Result_t(0, 0);
		}

		// assigning Any to something
		if(rhs->getType()->isAnyType() && !lhs->getType()->isAnyType())
		{
			// todo: find some fucking way to unwrap this shit at compile time.
			warn(user, "Unchecked assignment from 'any' to typed variable (unfixable)");

			iceAssert(rhsPtr);
			Result_t res = cgi->extractValueFromAny(user, rhsPtr, lhs->getType());

			cgi->irb.CreateStore(res.value, lhsPtr);

			// assign returns nothing.
			return Result_t(0, 0);
		}


		// assign fixed array to dynamic array
		if(lhs->getType()->isDynamicArrayType() && rhs->getType()->isArrayType()
			&& lhs->getType()->toDynamicArrayType()->getElementType() == rhs->getType()->toArrayType()->getElementType())
		{
			// make a copy of the array first
			// 1. calculate size

			fir::Value* alloclen = cgi->irb.CreateMul(cgi->irb.CreateSizeof(lhs->getType()->toDynamicArrayType()->getElementType()),
				fir::ConstantInt::getInt64(rhs->getType()->toArrayType()->getArraySize()));

			// 2. get malloc() fn
			fir::Function* mallocf = cgi->getOrDeclareLibCFunc(ALLOCATE_MEMORY_FUNC);
			iceAssert(mallocf);

			// 3. allocate memory
			fir::Value* allocmem = cgi->irb.CreateCall1(mallocf, alloclen);

			// 4. get the pointer on the rhs
			if(!rhsPtr) rhsPtr = cgi->irb.CreateImmutStackAlloc(rhs->getType(), rhs);
			fir::Value* arrptr = cgi->irb.CreateConstGEP2(rhsPtr, 0, 0);

			// 5. memcpy
			fir::Function* memcpyf = cgi->module->getIntrinsicFunction("memmove");

			cgi->irb.CreateCall(memcpyf, { cgi->irb.CreatePointerTypeCast(allocmem, fir::Type::getInt8Ptr()),
				cgi->irb.CreatePointerTypeCast(arrptr, fir::Type::getInt8Ptr()), alloclen, fir::ConstantInt::getInt32(0),
				fir::ConstantInt::getBool(0) });

			// 6. set the things
			fir::Type* elmty = lhs->getType()->toDynamicArrayType()->getElementType();
			cgi->irb.CreateSetDynamicArrayData(lhsPtr, cgi->irb.CreatePointerTypeCast(allocmem, elmty->getPointerTo()));
			cgi->irb.CreateSetDynamicArrayLength(lhsPtr, fir::ConstantInt::getInt64(rhs->getType()->toArrayType()->getArraySize()));
			cgi->irb.CreateSetDynamicArrayCapacity(lhsPtr, fir::ConstantInt::getInt64(rhs->getType()->toArrayType()->getArraySize()));

			// 7. do rc stuff
			if(cgi->isRefCountedType(lhs->getType()))
				cgi->assignRefCountedExpression(user, rhs, rhsPtr, lhs, lhsPtr, rhsvk, isInitial, true);

			return Result_t(0, 0);
		}


		// assign (constant) dynamic array to fixed array
		if(lhs->getType()->isArrayType() && rhs->getType()->isDynamicArrayType())
		{
			// see if we can do anything about it
			if(fir::ConstantDynamicArray* cda = dynamic_cast<fir::ConstantDynamicArray*>(rhs))
			{
				std::function<fir::ConstantArray*(fir::ConstantDynamicArray* dyn)> recursivelyConvertArray
					= [user, cgi, &recursivelyConvertArray](fir::ConstantDynamicArray* dyn) -> fir::ConstantArray* {

					std::vector<fir::ConstantValue*> values;
					if(!dyn->getArray())
						error(user, "Failed to convert array literal to fixed array");

					for(auto v : dyn->getArray()->getValues())
					{
						if(auto nested = dynamic_cast<fir::ConstantDynamicArray*>(v))
							values.push_back(recursivelyConvertArray(nested));

						else
							values.push_back(v);
					}

					iceAssert(values.size() > 0);
					auto type = fir::ArrayType::get(values[0]->getType(), values.size());

					return fir::ConstantArray::get(type, values);
				};

				fir::ConstantArray* arr = recursivelyConvertArray(cda);
				iceAssert(arr);

				// ok, check the type

				if(arr->getType() != lhs->getType())
				{
					error("Cannot assign array literal with type '%s' to type '%s'", arr->getType()->str().c_str(),
						lhs->getType()->str().c_str());
				}

				// rewrite history
				rhs = arr;
			}
			else
			{
				error(user, "Conversion from dynamic arrays to fixed arrays can only be done for constant array literals");
			}
		}








		// do the casting.
		if(lhsPtr->getType()->getPointerElementType() != rhs->getType())
		{
			rhs = cgi->autoCastType(lhsPtr->getType()->getPointerElementType(), rhs);
		}

		if(lhs->getType() != rhs->getType())
		{
			error(user, "Invalid assignment from value of type '%s' to one of type '%s'", rhs->getType()->str().c_str(),
				lhs->getType()->str().c_str());
		}

		if(cgi->isRefCountedType(lhs->getType()))
		{
			cgi->assignRefCountedExpression(user, rhs, rhsPtr, lhs, lhsPtr, rhsvk, isInitial, true);
			return Result_t(0, 0);
		}

		iceAssert(rhs);
		iceAssert(lhsPtr);

		cgi->irb.CreateStore(rhs, lhsPtr);

		return Result_t(0, 0);
	}
}
















