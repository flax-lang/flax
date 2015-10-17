// ExprCodeGen.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include <cinttypes>

#include "../include/ast.h"
#include "../include/codegen.h"
#include "../include/parser.h"

using namespace Ast;
using namespace Codegen;

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

	this->autoCastType(lhs, rhs);

	fir::Value* varptr = 0;
	if((v = dynamic_cast<VarRef*>(left)))
	{
		{
			VarDecl* vdecl = this->getSymDecl(user, v->name);
			if(!vdecl) GenError::unknownSymbol(this, user, v->name, SymbolType::Variable);

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







		// try and see if we have operator overloads for bo thing
		Result_t tryOpOverload = callOperatorOverloadOnStruct(this, user, op, ref, rhs, rhsPtr);
		if(tryOpOverload.result.first != 0)
			return tryOpOverload;

		this->autoCastType(rhs, lhs);
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

				// except llvm gives us nice checking.

				// fir::Value* r = this->builder.CreateConstGEP2_32(rhsPtr, 0, 0, "doStuff");
				fir::Value* r = this->builder.CreateGetPointer(rhsPtr,
					fir::ConstantInt::getUnsigned(fir::PrimitiveType::getUint64(this->getContext()), 0));

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
				fir::Value* lgep = this->builder.CreateGetConstStructMember(ref, 0);
				fir::Value* rgep = this->builder.CreateGetConstStructMember(rptr, 0);

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
					fir::Value* gep = this->builder.CreateGetConstStructMember(ai, 0);

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
				warn(user, "Value '%" PRIu64 "' is too large for variable type '%s', max %lld", n->ival, this->getReadableType(lhs->getType()).c_str(), max);
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
		// get the llvm op
		fir::Value* newrhs = this->builder.CreateBinaryOp(op, lhs, rhs);
		iceAssert(newrhs);

		this->builder.CreateStore(newrhs, varptr);
		return Result_t(newrhs, varptr);
	}
}



































Result_t BinOp::codegen(CodegenInstance* cgi, fir::Value* _lhsPtr, fir::Value* _rhs)
{
	iceAssert(this->left && this->right);
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
		// todo: fix???
		// was doing lhs without rhs, then rhs with lhs *before*.
		// swapped order to fix computed property setters.

		auto res = this->right->codegen(cgi /*, valptr.second*/).result;

		valptr = this->left->codegen(cgi, 0, res.first).result;


		rhs = res.first;
		lhs = valptr.first;

		fir::Value* rhsPtr = res.second;

		cgi->autoCastType(lhs, rhs, rhsPtr);
		return cgi->doBinOpAssign(this, this->left, this->right, this->op, lhs, valptr.second, rhs, rhsPtr);
	}
	else if(this->op == ArithmeticOp::Cast || this->op == ArithmeticOp::ForcedCast)
	{
		valptr = this->left->codegen(cgi).result;
		lhs = valptr.first;

		fir::Type* rtype = cgi->getLlvmType(this->right);
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

					fir::Value* gep = cgi->builder.CreateGetConstStructMember(tmp, 0);
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
				fir::Value* gep = cgi->builder.CreateGetConstStructMember(valptr.second, 0);
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

			fir::Value* stringPtr = cgi->builder.CreateGetConstStructMember(lhsref, 0);
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
	cgi->autoCastType(lhs, rhs, r.second);

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
		// fir::Instruction::BinaryOps lop = cgi->getBinaryOperator(this->op,
		// 	cgi->isSignedType(this->left) || cgi->isSignedType(this->right), false);

		cgi->autoCastType(lhs, rhs);
		cgi->autoCastType(rhs, lhs);

		fir::Value* tryop = cgi->builder.CreateBinaryOp(this->op, lhs, rhs);
		if(tryop)
		{
			return Result_t(tryop, 0);
		}

		cgi->autoCastType(rhs, lhs);

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

			case ArithmeticOp::LogicalOr:
				ret = cgi->builder.CreateLogicalOR(lhs, rhs);
				break;

			case ArithmeticOp::LogicalAnd:
				ret = cgi->builder.CreateLogicalAND(lhs, rhs);
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
		cgi->autoCastType(lhs, rhs, r.second);

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

			// logical shit.
			case ArithmeticOp::LogicalAnd:	return Result_t(cgi->builder.CreateLogicalAND(lhs, rhs), 0);
			case ArithmeticOp::LogicalOr:	return Result_t(cgi->builder.CreateLogicalOR(lhs, rhs), 0);

			default:						error(this, "Unsupported operator. (%d)", this->op);
		}
	}
	else if(cgi->isEnum(lhs->getType()) && cgi->isEnum(rhs->getType()))
	{
		iceAssert(lhsPtr);
		iceAssert(rhsPtr);

		fir::Value* gepL = cgi->builder.CreateGetConstStructMember(lhsPtr, 0);
		fir::Value* gepR = cgi->builder.CreateGetConstStructMember(rhsPtr, 0);

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
		// Result_t ret = cgi->findAndCallOperatorOverload(this, op, valptr.first, valptr.second, rhs, r.second);

		auto data = cgi->getOperatorOverload(this, op, valptr.first->getType(), rhs->getType());
		Result_t ret = cgi->callOperatorOverload(data, valptr.first, valptr.second, rhs, r.second, op);


		if(ret.result.first == 0)
		{
			error(this, "No such operator '%s' for expression %s %s %s", Parser::arithmeticOpToString(cgi, op).c_str(),
				cgi->getReadableType(lhs).c_str(), Parser::arithmeticOpToString(cgi, op).c_str(), cgi->getReadableType(rhs).c_str());
		}

		return ret;
	}

	error(this, "Unsupported operator on type");
}





