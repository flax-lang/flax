// ExprCodeGen.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include <cinttypes>
#include "../include/ast.h"
#include "../include/codegen.h"
#include "../include/llvm_all.h"
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
		Result_t ret = cgi->callOperatorOnStruct(tp, structRef, op, rhs, false);
		if(ret.result.first != 0)
		{
			return ret;
		}
		else if(op != ArithmeticOp::Assign)
		{
			// only assign can conceivably be done automatically
			GenError::noOpOverload(cgi, user, ((Struct*) tp->second.first)->name, op);
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
	ArrayIndex* ai	= nullptr;
	BinOp* bo		= nullptr;

	this->autoCastType(lhs, rhs);

	llvm::Value* varptr = 0;
	if((v = dynamic_cast<VarRef*>(left)))
	{
		{
			VarDecl* vdecl = this->getSymDecl(user, v->name);
			if(!vdecl) GenError::unknownSymbol(this, user, v->name, SymbolType::Variable);

			if(vdecl->immutable)
				error(this, user, "Cannot assign to immutable variable '%s'!", v->name.c_str());
		}

		if(!rhs)
			error(user, "(%s:%d) -> Internal check failed: invalid RHS for assignment", __FILE__, __LINE__);

		SymbolValidity_t sv = this->getSymPair(user, v->name)->first;
		if(sv.second != SymbolValidity::Valid)
			GenError::useAfterFree(this, user, v->name);

		else
			varptr = sv.first;

		if(!varptr)
			GenError::unknownSymbol(this, user, v->name, SymbolType::Variable);




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
			warn(this, left, "Unsigned assignment from 'Any' to typed variable (unfixable)");

			Result_t res = this->extractValueFromAny(lhs->getType(), rhsPtr);
			return Result_t(this->mainBuilder.CreateStore(res.result.first, ref), ref);
		}







		// try and see if we have operator overloads for bo thing
		Result_t tryOpOverload = callOperatorOverloadOnStruct(this, user, op, ref, rhs, right);
		if(tryOpOverload.result.first != 0)
			return tryOpOverload;

		if(lhs->getType() != rhs->getType())
		{
			// ensure we can always store 0 to pointers without a cast
			Number* n = 0;
			if(rhs->getType()->isIntegerTy() && (n = dynamic_cast<Number*>(right)) && n->ival == 0)
			{
				rhs = llvm::Constant::getNullValue(varptr->getType()->getPointerElementType());
			}
			else if(lhs->getType()->isPointerTy() && rhs->getType()->isIntegerTy())
			{
				// do pointer arithmetic.
				auto res = this->doPointerArithmetic(op, lhs, ref, rhs).result;
				this->mainBuilder.CreateStore(res.first, ref);

				return Result_t(res.first, res.second);
			}
			else
			{
				GenError::invalidAssignment(this, user, lhs, rhs);
			}
		}
		else if(this->isEnum(lhs->getType()))
		{
			warn(this, left, "enum assign\n");
		}
	}
	else if((dynamic_cast<MemberAccess*>(left))
		|| ((uo = dynamic_cast<UnaryOp*>(left)) && uo->op == ArithmeticOp::Deref)
		|| (ai = dynamic_cast<ArrayIndex*>(left)))
	{
		// we know that the ptr lives in the second element
		// so, use it

		varptr = ref;
		iceAssert(rhs);

		// make sure the left side is a pointer
		if(!varptr)
			error(this, user, "Cannot assign to immutable variable");

		else if(!varptr->getType()->isPointerTy())
			GenError::invalidAssignment(this, user, varptr, rhs);

		// redo the number casting
		if(rhs->getType()->isIntegerTy() && lhs->getType()->isIntegerTy())
			rhs = this->mainBuilder.CreateIntCast(rhs, varptr->getType()->getPointerElementType(), false);

		else if(rhs->getType()->isIntegerTy() && lhs->getType()->isPointerTy())
			rhs = this->mainBuilder.CreateIntToPtr(rhs, lhs->getType());
	}
	else if((bo = dynamic_cast<BinOp*>(left)) && bo->op == ArithmeticOp::MemberAccess)
	{
		// great job, folks
		// printf("(%s:%lld): dot operator as LHS of op\n", bo->posinfo.file.c_str(), bo->posinfo.line);
		MemberAccess* fakema = new MemberAccess(bo->posinfo, bo->left, bo->right);
		BinOp* fakebo = new BinOp(bo->posinfo, fakema, op, right);

		Result_t res = fakebo->codegen(this);

		delete fakema;
		delete fakebo;

		return res;
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

		this->mainBuilder.CreateStore(rhs, varptr);
		return Result_t(rhs, varptr);
	}
	else
	{
		// get the llvm op
		llvm::Instruction::BinaryOps lop = this->getBinaryOperator(op, this->isSignedType(left) || this->isSignedType(right), lhs->getType()->isFloatingPointTy() || rhs->getType()->isFloatingPointTy());

		llvm::Value* newrhs = this->mainBuilder.CreateBinOp(lop, lhs, rhs);
		this->mainBuilder.CreateStore(newrhs, varptr);
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
		// todo: somehow solve a circular dependency of lhs <> rhs
		valptr = this->left->codegen(cgi).result;

		auto res = this->right->codegen(cgi, valptr.second).result;
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
			warn(cgi, this, "Redundant cast");
			return Result_t(lhs, 0);
		}

		if(this->op != ArithmeticOp::ForcedCast)
		{
			if(lhs->getType()->isIntegerTy() && rtype->isIntegerTy())
			{
				return Result_t(cgi->mainBuilder.CreateIntCast(lhs, rtype, cgi->isSignedType(this->left)), 0);
			}
			else if(lhs->getType()->isIntegerTy() && rtype->isFloatingPointTy())
			{
				return Result_t(cgi->mainBuilder.CreateSIToFP(lhs, rtype), 0);
			}
			else if(lhs->getType()->isFloatingPointTy() && rtype->isFloatingPointTy())
			{
				printf("float to float: %d -> %d\n", lhs->getType()->getPrimitiveSizeInBits(), rtype->getPrimitiveSizeInBits());
				if(lhs->getType()->getPrimitiveSizeInBits() > rtype->getPrimitiveSizeInBits())
					return Result_t(cgi->mainBuilder.CreateFPTrunc(lhs, rtype), 0);

				else
					return Result_t(cgi->mainBuilder.CreateFPExt(lhs, rtype), 0);
			}
			else if(lhs->getType()->isPointerTy() && rtype->isPointerTy())
			{
				return Result_t(cgi->mainBuilder.CreatePointerCast(lhs, rtype), 0);
			}
			else if(lhs->getType()->isPointerTy() && rtype->isIntegerTy())
			{
				return Result_t(cgi->mainBuilder.CreatePtrToInt(lhs, rtype), 0);
			}
			else if(lhs->getType()->isIntegerTy() && rtype->isPointerTy())
			{
				return Result_t(cgi->mainBuilder.CreateIntToPtr(lhs, rtype), 0);
			}
			else if(cgi->isEnum(rtype))
			{
				// dealing with enum
				llvm::Type* insideType = rtype->getStructElementType(0);
				if(lhs->getType() == insideType)
				{
					llvm::Value* tmp = cgi->allocateInstanceInBlock(rtype, "tmp_enum");

					llvm::Value* gep = cgi->mainBuilder.CreateStructGEP(tmp, 0, "castedAndWrapped");
					cgi->mainBuilder.CreateStore(lhs, gep);

					return Result_t(cgi->mainBuilder.CreateLoad(tmp), tmp);
				}
				else
				{
					error(cgi, this, "Enum '%s' does not have type '%s', invalid cast", rtype->getStructName().str().c_str(),
						cgi->getReadableType(lhs).c_str());
				}
			}
			else if(cgi->isEnum(lhs->getType()) && lhs->getType()->getStructElementType(0) == rtype)
			{
				iceAssert(valptr.second);
				llvm::Value* gep = cgi->mainBuilder.CreateStructGEP(valptr.second, 0, "castedAndWrapped");
				llvm::Value* val = cgi->mainBuilder.CreateLoad(gep);

				return Result_t(val, gep);
			}
		}



		if(cgi->isAnyType(lhs->getType()))
		{
			iceAssert(valptr.second);
			return cgi->extractValueFromAny(rtype, valptr.second);
		}
		else if(this->op != ArithmeticOp::ForcedCast)
		{
			warn(cgi, this, "Unknown cast, doing raw bitcast (from type %s to %s)", cgi->getReadableType(lhs->getType()).c_str(), cgi->getReadableType(rtype).c_str());
		}

		return Result_t(cgi->mainBuilder.CreateBitCast(lhs, rtype), 0);
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
		error(cgi, this, "lhs null");

	if(!rhs)
	{
		printf("rhs: %s\n", typeid(*this->right).name());
		error(cgi, this, "rhs null");
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
				llvm::Value* casted = cgi->mainBuilder.CreatePtrToInt(lhs, rhs->getType());

				if(this->op == ArithmeticOp::CmpEq)
					return Result_t(cgi->mainBuilder.CreateICmpEQ(casted, rhs), 0);
				else
					return Result_t(cgi->mainBuilder.CreateICmpNE(casted, rhs), 0);
			}
		}
	}
	else if(lhs->getType()->isIntegerTy() && rhs->getType()->isIntegerTy())
	{
		llvm::Instruction::BinaryOps lop = cgi->getBinaryOperator(this->op,
			cgi->isSignedType(this->left) || cgi->isSignedType(this->right), false);

		if(lop != (llvm::Instruction::BinaryOps) 0)
		{
			cgi->autoCastType(lhs, rhs);
			cgi->autoCastType(rhs, lhs);

			if(lhs->getType() != rhs->getType())
			{
				error(cgi, this, "Invalid binary op between '%s' and '%s'", cgi->getReadableType(lhs).c_str(),
					cgi->getReadableType(rhs).c_str());
			}

			return Result_t(cgi->mainBuilder.CreateBinOp(lop, lhs, rhs), 0);
		}

		cgi->autoCastType(rhs, lhs);
		switch(this->op)
		{
			// comparisons
			case ArithmeticOp::CmpEq:		return Result_t(cgi->mainBuilder.CreateICmpEQ(lhs, rhs), 0);
			case ArithmeticOp::CmpNEq:		return Result_t(cgi->mainBuilder.CreateICmpNE(lhs, rhs), 0);


			case ArithmeticOp::CmpLT:
				if(cgi->isSignedType(this->left) || cgi->isSignedType(this->right))
					return Result_t(cgi->mainBuilder.CreateICmpSLT(lhs, rhs), 0);
				else
					return Result_t(cgi->mainBuilder.CreateICmpULT(lhs, rhs), 0);



			case ArithmeticOp::CmpGT:
				if(cgi->isSignedType(this->left) || cgi->isSignedType(this->right))
					return Result_t(cgi->mainBuilder.CreateICmpSGT(lhs, rhs), 0);
				else
					return Result_t(cgi->mainBuilder.CreateICmpUGT(lhs, rhs), 0);


			case ArithmeticOp::CmpLEq:
				if(cgi->isSignedType(this->left) || cgi->isSignedType(this->right))
					return Result_t(cgi->mainBuilder.CreateICmpSLE(lhs, rhs), 0);
				else
					return Result_t(cgi->mainBuilder.CreateICmpULE(lhs, rhs), 0);



			case ArithmeticOp::CmpGEq:
				if(cgi->isSignedType(this->left) || cgi->isSignedType(this->right))
					return Result_t(cgi->mainBuilder.CreateICmpSGE(lhs, rhs), 0);
				else
					return Result_t(cgi->mainBuilder.CreateICmpUGE(lhs, rhs), 0);


			case ArithmeticOp::LogicalOr:
			case ArithmeticOp::LogicalAnd:
			{
				int theOp = this->op == ArithmeticOp::LogicalOr ? 0 : 1;
				llvm::Value* trueval = llvm::ConstantInt::get(cgi->getContext(), llvm::APInt(1, 1, true));
				llvm::Value* falseval = llvm::ConstantInt::get(cgi->getContext(), llvm::APInt(1, 0, true));


				llvm::Function* func = cgi->mainBuilder.GetInsertBlock()->getParent();
				iceAssert(func);

				llvm::Value* res = cgi->mainBuilder.CreateTrunc(lhs, llvm::Type::getInt1Ty(cgi->getContext()));

				llvm::BasicBlock* entry = cgi->mainBuilder.GetInsertBlock();
				llvm::BasicBlock* lb = llvm::BasicBlock::Create(cgi->getContext(), "leftbl", func);
				llvm::BasicBlock* rb = llvm::BasicBlock::Create(cgi->getContext(), "rightbl", func);
				llvm::BasicBlock* mb = llvm::BasicBlock::Create(cgi->getContext(), "mergebl", func);
				cgi->mainBuilder.CreateCondBr(res, lb, rb);


				cgi->mainBuilder.SetInsertPoint(rb);
				// this kinda works recursively
				if(!this->phi)
					this->phi = cgi->mainBuilder.CreatePHI(llvm::Type::getInt1Ty(cgi->getContext()), 2);


				// if this is a logical-or
				if(theOp == 0)
				{
					// do the true case
					cgi->mainBuilder.SetInsertPoint(lb);
					this->phi->addIncoming(trueval, lb);

					// if it succeeded (aka res is true), go to the merge block.
					cgi->mainBuilder.CreateBr(rb);



					// do the false case
					cgi->mainBuilder.SetInsertPoint(rb);

					// do another compare.
					llvm::Value* rres = cgi->mainBuilder.CreateTrunc(rhs, llvm::Type::getInt1Ty(cgi->getContext()));
					this->phi->addIncoming(rres, entry);
				}
				else
				{
					// do the true case
					cgi->mainBuilder.SetInsertPoint(lb);
					llvm::Value* rres = cgi->mainBuilder.CreateTrunc(rhs, llvm::Type::getInt1Ty(cgi->getContext()));
					this->phi->addIncoming(rres, lb);

					cgi->mainBuilder.CreateBr(rb);


					// do the false case
					cgi->mainBuilder.SetInsertPoint(rb);
					phi->addIncoming(falseval, entry);
				}

				cgi->mainBuilder.CreateBr(mb);
				cgi->mainBuilder.SetInsertPoint(mb);

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
			error(cgi, this, "Left and right-hand side of binary expression do not have have the same type! (%s vs %s)", cgi->getReadableType(lhs).c_str(), cgi->getReadableType(rhs).c_str());

		// then they're floats.
		switch(this->op)
		{
			case ArithmeticOp::Add:			return Result_t(cgi->mainBuilder.CreateFAdd(lhs, rhs), 0);
			case ArithmeticOp::Subtract:	return Result_t(cgi->mainBuilder.CreateFSub(lhs, rhs), 0);
			case ArithmeticOp::Multiply:	return Result_t(cgi->mainBuilder.CreateFMul(lhs, rhs), 0);
			case ArithmeticOp::Divide:		return Result_t(cgi->mainBuilder.CreateFDiv(lhs, rhs), 0);

			// comparisons
			case ArithmeticOp::CmpEq:		return Result_t(cgi->mainBuilder.CreateFCmpOEQ(lhs, rhs), 0);
			case ArithmeticOp::CmpNEq:		return Result_t(cgi->mainBuilder.CreateFCmpONE(lhs, rhs), 0);
			case ArithmeticOp::CmpLT:		return Result_t(cgi->mainBuilder.CreateFCmpOLT(lhs, rhs), 0);
			case ArithmeticOp::CmpGT:		return Result_t(cgi->mainBuilder.CreateFCmpOGT(lhs, rhs), 0);
			case ArithmeticOp::CmpLEq:		return Result_t(cgi->mainBuilder.CreateFCmpOLE(lhs, rhs), 0);
			case ArithmeticOp::CmpGEq:		return Result_t(cgi->mainBuilder.CreateFCmpOGE(lhs, rhs), 0);

			default:						error(cgi, this, "Unsupported operator.");
		}
	}
	else if(cgi->isEnum(lhs->getType()) && cgi->isEnum(rhs->getType()))
	{
		iceAssert(lhsPtr);
		iceAssert(rhsPtr);

		llvm::Value* gepL = cgi->mainBuilder.CreateStructGEP(lhsPtr, 0);
		llvm::Value* gepR = cgi->mainBuilder.CreateStructGEP(rhsPtr, 0);

		llvm::Value* l = cgi->mainBuilder.CreateLoad(gepL);
		llvm::Value* r = cgi->mainBuilder.CreateLoad(gepR);

		llvm::Value* res = 0;

		if(this->op == ArithmeticOp::CmpEq)
		{
			res = cgi->mainBuilder.CreateICmpEQ(l, r);
		}
		else if(this->op == ArithmeticOp::CmpNEq)
		{
			res = cgi->mainBuilder.CreateICmpNE(l, r);
		}
		else
		{
			error(cgi, this, "Invalid comparison %s on Type!", Parser::arithmeticOpToString(this->op).c_str());
		}


		return Result_t(res, 0);
	}
	else if(lhs->getType()->isStructTy())
	{
		TypePair_t* p = cgi->getType(lhs->getType()->getStructName());
		if(!p)
		{
			error(cgi, this, "Invalid type");
		}

		return cgi->callOperatorOnStruct(p, valptr.second, op, rhs);
	}

	error(cgi, this, "Unsupported operator on type");
}





