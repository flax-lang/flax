// ExprCodeGen.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include <cinttypes>

#include "../include/ast.h"
#include "../include/codegen.h"
#include "../include/parser.h"

using namespace Ast;
using namespace Codegen;

static Result_t callOperatorOverloadOnStruct(CodegenInstance* cgi, Expr* user, ArithmeticOp op, llvm::Value* structRef, llvm::Value* rhs, Ast::Expr* rhsExpr)
{
	if(structRef->getType()->getPointerElementType()->isStructTy())
	{
		TypePair_t* tp = cgi->getType(structRef->getType()->getPointerElementType()->getStructName());
		if(!tp)
			return Result_t(0, 0);

		// if we can find an operator, then we call it. if not, then we'll have to handle it somewhere below.
		Result_t ret = cgi->callOperatorOnStruct(user, tp, structRef, op, rhs, false);
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



Result_t CodegenInstance::doBinOpAssign(Expr* user, Expr* left, Expr* right, ArithmeticOp op, llvm::Value* lhs,
	llvm::Value* ref, llvm::Value* rhs, llvm::Value* rhsPtr)
{
	VarRef* v		= nullptr;
	UnaryOp* uo		= nullptr;
	ArrayIndex* ari	= nullptr;

	this->autoCastType(lhs, rhs);

	llvm::Value* varptr = 0;
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
		Result_t tryOpOverload = callOperatorOverloadOnStruct(this, user, op, ref, rhs, right);
		if(tryOpOverload.result.first != 0)
			return tryOpOverload;

		if(!this->areEqualTypes(lhs->getType(), rhs->getType()))
		{
			// ensure we can always store 0 to pointers without a cast
			Number* n = 0;
			if(op == ArithmeticOp::Assign && rhs->getType()->isIntegerTy() && (n = dynamic_cast<Number*>(right)) && n->ival == 0)
			{
				rhs = llvm::Constant::getNullValue(varptr->getType()->getPointerElementType());
			}
			else if(lhs->getType()->isPointerTy() && rhs->getType()->isIntegerTy())
			{
				// do pointer arithmetic.
				auto res = this->doPointerArithmetic(op, lhs, ref, rhs).result;
				this->builder.CreateStore(res.first, ref);

				return Result_t(res.first, res.second);
			}
			else if(lhs->getType()->isPointerTy() && rhs->getType()->isArrayTy()
				&& (lhs->getType()->getPointerElementType() == rhs->getType()->getArrayElementType()))
			{
				// this GEP is needed to convert the "pointer-to-array" into a "pointer-to-element"
				// ie. do what C does implicitly, where the array is really just a pointer to
				// the first element.

				// except llvm gives us nice checking.

				llvm::Value* r = this->builder.CreateConstGEP2_32(rhsPtr, 0, 0, "doStuff");
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
				llvm::Value* rptr = rhsPtr;
				if(!rptr)
				{
					// fuck it, create a temporary.
					llvm::Value* temp = this->allocateInstanceInBlock(rhs->getType());
					iceAssert(temp->getType()->getPointerElementType()->isStructTy());

					this->builder.CreateStore(rhs, temp);
					rptr = temp;
				}

				iceAssert(rptr);
				iceAssert(this->areEqualTypes(lhs->getType(), rhs->getType()));

				// put the rhs thingy into the lhs.
				llvm::Value* lgep = this->builder.CreateStructGEP(ref, 0);
				llvm::Value* rgep = this->builder.CreateStructGEP(rptr, 0);

				iceAssert(lgep->getType() == rgep->getType());
				llvm::Value* rval = this->builder.CreateLoad(rgep);

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
				else if(lhs->getType()->getStructElementType(0) != rhs->getType())
				{
					error(right, "Assigning to assign value with type %s to enumeration with base type %s",
						this->getReadableType(rhs).c_str(), this->getReadableType(lhs).c_str());
				}
				else
				{
					// okay.
					// create a wrapper value.

					llvm::Value* ai = this->allocateInstanceInBlock(lhs->getType());
					llvm::Value* gep = this->builder.CreateStructGEP(ai, 0);

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

		else if(!varptr->getType()->isPointerTy())
			GenError::invalidAssignment(this, user, varptr, rhs);

		// redo the number casting
		if(rhs->getType()->isIntegerTy() && lhs->getType()->isIntegerTy())
			rhs = this->builder.CreateIntCast(rhs, varptr->getType()->getPointerElementType(), false);

		else if(rhs->getType()->isIntegerTy() && lhs->getType()->isPointerTy())
			rhs = this->builder.CreateIntToPtr(rhs, lhs->getType());
	}
	else
	{
		error(user, "Left-hand side of assignment must be assignable (type: %s)", typeid(*left).name());
	}

	if(varptr->getType()->getPointerElementType()->isStructTy())
	{
		Result_t tryOpOverload = callOperatorOverloadOnStruct(this, user, op, varptr, rhs, right);
		if(tryOpOverload.result.first != 0)
			return tryOpOverload;
	}

	// check for overflow
	if(lhs->getType()->isIntegerTy())
	{
		Number* n = 0;
		uint64_t max = 1;

		// why the fuck does c++ not have a uint64_t pow function
		{
			for(unsigned int i = 0; i < lhs->getType()->getIntegerBitWidth(); i++)
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

		// printf("%s -> %s\n", this->getReadableType(rhs).c_str(), this->getReadableType(varptr).c_str());
		this->builder.CreateStore(rhs, varptr);
		return Result_t(rhs, varptr);
	}
	else
	{
		// get the llvm op
		llvm::Instruction::BinaryOps lop = this->getBinaryOperator(op, this->isSignedType(left) || this->isSignedType(right), lhs->getType()->isFloatingPointTy() || rhs->getType()->isFloatingPointTy());

		llvm::Value* newrhs = this->builder.CreateBinOp(lop, lhs, rhs);
		this->builder.CreateStore(newrhs, varptr);
		return Result_t(newrhs, varptr);
	}
}



































Result_t BinOp::codegen(CodegenInstance* cgi, llvm::Value* _lhsPtr, llvm::Value* _rhs)
{
	iceAssert(this->left && this->right);
	ValPtr_t valptr;

	llvm::Value* lhs;
	llvm::Value* rhs;

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

		llvm::Value* rhsPtr = res.second;

		cgi->autoCastType(lhs, rhs, rhsPtr);
		return cgi->doBinOpAssign(this, this->left, this->right, this->op, lhs, valptr.second, rhs, rhsPtr);
	}
	else if(this->op == ArithmeticOp::Cast || this->op == ArithmeticOp::ForcedCast)
	{
		valptr = this->left->codegen(cgi).result;
		lhs = valptr.first;

		llvm::Type* rtype = cgi->getLlvmType(this->right);
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
			if(lhs->getType()->isIntegerTy() && rtype->isIntegerTy())
			{
				return Result_t(cgi->builder.CreateIntCast(lhs, rtype, cgi->isSignedType(this->left)), 0);
			}
			else if(lhs->getType()->isIntegerTy() && rtype->isFloatingPointTy())
			{
				return Result_t(cgi->builder.CreateSIToFP(lhs, rtype), 0);
			}
			else if(lhs->getType()->isFloatingPointTy() && rtype->isFloatingPointTy())
			{
				printf("float to float: %d -> %d\n", lhs->getType()->getPrimitiveSizeInBits(), rtype->getPrimitiveSizeInBits());
				if(lhs->getType()->getPrimitiveSizeInBits() > rtype->getPrimitiveSizeInBits())
					return Result_t(cgi->builder.CreateFPTrunc(lhs, rtype), 0);

				else
					return Result_t(cgi->builder.CreateFPExt(lhs, rtype), 0);
			}
			else if(lhs->getType()->isPointerTy() && rtype->isPointerTy())
			{
				return Result_t(cgi->builder.CreatePointerCast(lhs, rtype), 0);
			}
			else if(lhs->getType()->isPointerTy() && rtype->isIntegerTy())
			{
				return Result_t(cgi->builder.CreatePtrToInt(lhs, rtype), 0);
			}
			else if(lhs->getType()->isIntegerTy() && rtype->isPointerTy())
			{
				return Result_t(cgi->builder.CreateIntToPtr(lhs, rtype), 0);
			}
			else if(cgi->isEnum(rtype))
			{
				// dealing with enum
				llvm::Type* insideType = rtype->getStructElementType(0);
				if(lhs->getType() == insideType)
				{
					llvm::Value* tmp = cgi->allocateInstanceInBlock(rtype, "tmp_enum");

					llvm::Value* gep = cgi->builder.CreateStructGEP(tmp, 0, "castedAndWrapped");
					cgi->builder.CreateStore(lhs, gep);

					return Result_t(cgi->builder.CreateLoad(tmp), tmp);
				}
				else
				{
					error(this, "Enum '%s' does not have type '%s', invalid cast", rtype->getStructName().str().c_str(),
						cgi->getReadableType(lhs).c_str());
				}
			}
			else if(cgi->isEnum(lhs->getType()) && lhs->getType()->getStructElementType(0) == rtype)
			{
				iceAssert(valptr.second);
				llvm::Value* gep = cgi->builder.CreateStructGEP(valptr.second, 0, "castedAndWrapped");
				llvm::Value* val = cgi->builder.CreateLoad(gep);

				return Result_t(val, gep);
			}
		}



		if(cgi->isAnyType(lhs->getType()))
		{
			iceAssert(valptr.second);
			return cgi->extractValueFromAny(rtype, valptr.second);
		}
		else if(lhs->getType()->isStructTy() && lhs->getType()->getStructName() == "String"
			&& rtype == llvm::Type::getInt8PtrTy(cgi->getContext()))
		{
			auto strPair = cgi->getType(cgi->mangleWithNamespace("String", std::deque<std::string>()));
			llvm::StructType* stringType = llvm::cast<llvm::StructType>(strPair->first);

			// string to int8*.
			// just access the data pointer.

			llvm::Value* lhsref = valptr.second;
			if(!lhsref)
			{
				// dammit.
				lhsref = cgi->allocateInstanceInBlock(stringType);
				cgi->builder.CreateStore(lhs, lhsref);
			}

			llvm::Value* stringPtr = cgi->builder.CreateStructGEP(lhsref, 0);
			return Result_t(cgi->builder.CreateLoad(stringPtr), stringPtr);
		}
		else if(lhs->getType() == llvm::Type::getInt8PtrTy(cgi->getContext())
			&& rtype->isStructTy() && rtype->getStructName() == "String")
		{
			// support this shit.
			// error(cgi, this, "Automatic char* -> String casting not yet supported");

			// create a bogus func call.
			TypePair_t* tp = cgi->getType("String");
			iceAssert(tp);

			std::vector<llvm::Value*> args { lhs };
			return cgi->callTypeInitialiser(tp, this, args);
		}
		else if(this->op != ArithmeticOp::ForcedCast)
		{
			std::string lstr = cgi->getReadableType(lhs).c_str();
			std::string rstr = cgi->getReadableType(rtype).c_str();

			if(!llvm::CastInst::castIsValid(llvm::Instruction::BitCast, lhs, rtype))
			{
				error(this, "Invalid cast from type %s to %s", lstr.c_str(), rstr.c_str());
			}
			else
			{
				warn(this, "Unknown cast, doing raw bitcast (from type %s to %s)", lstr.c_str(), rstr.c_str());
			}
		}

		return Result_t(cgi->builder.CreateBitCast(lhs, rtype), 0);
	}
	else
	{
		valptr = this->left->codegen(cgi).result;
	}


	// else case.
	// no point being explicit about this and wasting indentation

	lhs = valptr.first;
	llvm::Value* lhsPtr = valptr.second;
	llvm::Value* lhsptr = valptr.second;
	auto r = this->right->codegen(cgi).result;

	rhs = r.first;
	llvm::Value* rhsPtr = r.second;
	cgi->autoCastType(lhs, rhs, r.second);

	// if adding integer to pointer
	if(!lhs)
		error(this, "lhs null");

	if(!rhs)
	{
		printf("rhs: %s\n", typeid(*this->right).name());
		error(this, "rhs null");
	}



	bool isBuiltinIntegerOp = false;
	{
		bool lhsInteger = false;
		bool rhsInteger = false;

		if(lhs->getType()->isIntegerTy())
		{
			lhsInteger = true;
		}

		if(rhs->getType()->isIntegerTy())
		{
			rhsInteger = true;

		}

		isBuiltinIntegerOp = (lhsInteger && rhsInteger);
	}
















	if(lhs->getType()->isPointerTy() && rhs->getType()->isIntegerTy())
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
				llvm::Value* casted = cgi->builder.CreatePtrToInt(lhs, rhs->getType());

				if(this->op == ArithmeticOp::CmpEq)
					return Result_t(cgi->builder.CreateICmpEQ(casted, rhs), 0);
				else
					return Result_t(cgi->builder.CreateICmpNE(casted, rhs), 0);
			}
		}
	}
	else if(isBuiltinIntegerOp)
	{
		llvm::Instruction::BinaryOps lop = cgi->getBinaryOperator(this->op,
			cgi->isSignedType(this->left) || cgi->isSignedType(this->right), false);

		if(lop != (llvm::Instruction::BinaryOps) 0)
		{
			cgi->autoCastType(lhs, rhs);
			cgi->autoCastType(rhs, lhs);

			if(lhs->getType() != rhs->getType())
			{
				error(this, "Invalid binary op between '%s' and '%s'", cgi->getReadableType(lhs).c_str(),
					cgi->getReadableType(rhs).c_str());
			}

			return Result_t(cgi->builder.CreateBinOp(lop, lhs, rhs), 0);
		}

		cgi->autoCastType(rhs, lhs);
		switch(this->op)
		{
			// comparisons
			case ArithmeticOp::CmpEq:		return Result_t(cgi->builder.CreateICmpEQ(lhs, rhs), 0);
			case ArithmeticOp::CmpNEq:		return Result_t(cgi->builder.CreateICmpNE(lhs, rhs), 0);


			case ArithmeticOp::CmpLT:
				if(cgi->isSignedType(this->left) || cgi->isSignedType(this->right))
					return Result_t(cgi->builder.CreateICmpSLT(lhs, rhs), 0);
				else
					return Result_t(cgi->builder.CreateICmpULT(lhs, rhs), 0);



			case ArithmeticOp::CmpGT:
				if(cgi->isSignedType(this->left) || cgi->isSignedType(this->right))
					return Result_t(cgi->builder.CreateICmpSGT(lhs, rhs), 0);
				else
					return Result_t(cgi->builder.CreateICmpUGT(lhs, rhs), 0);


			case ArithmeticOp::CmpLEq:
				if(cgi->isSignedType(this->left) || cgi->isSignedType(this->right))
					return Result_t(cgi->builder.CreateICmpSLE(lhs, rhs), 0);
				else
					return Result_t(cgi->builder.CreateICmpULE(lhs, rhs), 0);



			case ArithmeticOp::CmpGEq:
				if(cgi->isSignedType(this->left) || cgi->isSignedType(this->right))
					return Result_t(cgi->builder.CreateICmpSGE(lhs, rhs), 0);
				else
					return Result_t(cgi->builder.CreateICmpUGE(lhs, rhs), 0);


			case ArithmeticOp::LogicalOr:
			case ArithmeticOp::LogicalAnd:
			{
				int theOp = this->op == ArithmeticOp::LogicalOr ? 0 : 1;
				llvm::Value* trueval = llvm::ConstantInt::get(cgi->getContext(), llvm::APInt(1, 1, true));
				llvm::Value* falseval = llvm::ConstantInt::get(cgi->getContext(), llvm::APInt(1, 0, true));


				llvm::Function* func = cgi->builder.GetInsertBlock()->getParent();
				iceAssert(func);

				llvm::Value* res = cgi->builder.CreateTrunc(lhs, llvm::Type::getInt1Ty(cgi->getContext()));

				llvm::BasicBlock* entry = cgi->builder.GetInsertBlock();
				llvm::BasicBlock* lb = llvm::BasicBlock::Create(cgi->getContext(), "leftbl", func);
				llvm::BasicBlock* rb = llvm::BasicBlock::Create(cgi->getContext(), "rightbl", func);
				llvm::BasicBlock* mb = llvm::BasicBlock::Create(cgi->getContext(), "mergebl", func);
				cgi->builder.CreateCondBr(res, lb, rb);


				cgi->builder.SetInsertPoint(rb);
				// this kinda works recursively
				if(!this->phi)
					this->phi = cgi->builder.CreatePHI(llvm::Type::getInt1Ty(cgi->getContext()), 2);


				// if this is a logical-or
				if(theOp == 0)
				{
					// do the true case
					cgi->builder.SetInsertPoint(lb);
					this->phi->addIncoming(trueval, lb);

					// if it succeeded (aka res is true), go to the merge block.
					cgi->builder.CreateBr(rb);



					// do the false case
					cgi->builder.SetInsertPoint(rb);

					// do another compare.
					llvm::Value* rres = cgi->builder.CreateTrunc(rhs, llvm::Type::getInt1Ty(cgi->getContext()));
					this->phi->addIncoming(rres, entry);
				}
				else
				{
					// do the true case
					cgi->builder.SetInsertPoint(lb);
					llvm::Value* rres = cgi->builder.CreateTrunc(rhs, llvm::Type::getInt1Ty(cgi->getContext()));
					this->phi->addIncoming(rres, lb);

					cgi->builder.CreateBr(rb);


					// do the false case
					cgi->builder.SetInsertPoint(rb);
					phi->addIncoming(falseval, entry);
				}

				cgi->builder.CreateBr(mb);
				cgi->builder.SetInsertPoint(mb);

				return Result_t(this->phi, 0);
			}

			default:
				// should not be reached
				error("what?!");
		}
	}
	else if(cgi->isBuiltinType(this->left) && cgi->isBuiltinType(this->right))
	{
		// if one of them is an integer, cast it first
		cgi->autoCastType(lhs, rhs, r.second);

		if(lhs->getType() != rhs->getType())
			error(this, "Left and right-hand side of binary expression do not have have the same type! (%s vs %s)", cgi->getReadableType(lhs).c_str(), cgi->getReadableType(rhs).c_str());

		// then they're floats.
		switch(this->op)
		{
			case ArithmeticOp::Add:			return Result_t(cgi->builder.CreateFAdd(lhs, rhs), 0);
			case ArithmeticOp::Subtract:	return Result_t(cgi->builder.CreateFSub(lhs, rhs), 0);
			case ArithmeticOp::Multiply:	return Result_t(cgi->builder.CreateFMul(lhs, rhs), 0);
			case ArithmeticOp::Divide:		return Result_t(cgi->builder.CreateFDiv(lhs, rhs), 0);

			// comparisons
			case ArithmeticOp::CmpEq:		return Result_t(cgi->builder.CreateFCmpOEQ(lhs, rhs), 0);
			case ArithmeticOp::CmpNEq:		return Result_t(cgi->builder.CreateFCmpONE(lhs, rhs), 0);
			case ArithmeticOp::CmpLT:		return Result_t(cgi->builder.CreateFCmpOLT(lhs, rhs), 0);
			case ArithmeticOp::CmpGT:		return Result_t(cgi->builder.CreateFCmpOGT(lhs, rhs), 0);
			case ArithmeticOp::CmpLEq:		return Result_t(cgi->builder.CreateFCmpOLE(lhs, rhs), 0);
			case ArithmeticOp::CmpGEq:		return Result_t(cgi->builder.CreateFCmpOGE(lhs, rhs), 0);

			default:						error(this, "Unsupported operator.");
		}
	}
	else if(cgi->isEnum(lhs->getType()) && cgi->isEnum(rhs->getType()))
	{
		iceAssert(lhsPtr);
		iceAssert(rhsPtr);

		llvm::Value* gepL = cgi->builder.CreateStructGEP(lhsPtr, 0);
		llvm::Value* gepR = cgi->builder.CreateStructGEP(rhsPtr, 0);

		llvm::Value* l = cgi->builder.CreateLoad(gepL);
		llvm::Value* r = cgi->builder.CreateLoad(gepR);

		llvm::Value* res = 0;

		if(this->op == ArithmeticOp::CmpEq)
		{
			res = cgi->builder.CreateICmpEQ(l, r);
		}
		else if(this->op == ArithmeticOp::CmpNEq)
		{
			res = cgi->builder.CreateICmpNE(l, r);
		}
		else
		{
			error(this, "Invalid comparison %s on Type!", Parser::arithmeticOpToString(cgi, this->op).c_str());
		}


		return Result_t(res, 0);
	}
	else if(lhs->getType()->isStructTy())
	{
		TypePair_t* p = cgi->getType(lhs->getType()->getStructName());
		if(!p)
		{
			error(this, "Invalid type (%s)", lhs->getType()->getStructName().str().c_str());
		}


		if(valptr.second == 0)
		{
			// we don't have a pointer-type ref, which is required for operators to work.
			// create one.

			llvm::Value* val = valptr.first;
			iceAssert(val);

			llvm::Value* ptr = cgi->builder.CreateAlloca(val->getType());
			cgi->builder.CreateStore(val, ptr);

			valptr.second = ptr;
		}

		return cgi->callOperatorOnStruct(this, p, valptr.second, op, rhs);
	}

	error(this, "Unsupported operator on type");
}





