// ExprCodeGen.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "../include/ast.h"
#include "../include/codegen.h"
#include "../include/llvm_all.h"

#include <llvm/IR/DataLayout.h>

using namespace Ast;
using namespace Codegen;

Result_t UnaryOp::codegen(CodegenInstance* cgi)
{
	assert(this->expr);
	switch(this->op)
	{
		case ArithmeticOp::LogicalNot:
			return Result_t(cgi->mainBuilder.CreateNot(this->expr->codegen(cgi).result.first), 0);

		case ArithmeticOp::Minus:
			return Result_t(cgi->mainBuilder.CreateNeg(this->expr->codegen(cgi).result.first), 0);

		case ArithmeticOp::Plus:
			return this->expr->codegen(cgi);

		case ArithmeticOp::Deref:
		{
			Result_t vp = this->expr->codegen(cgi);
			return Result_t(cgi->mainBuilder.CreateLoad(vp.result.first), vp.result.first);
		}

		case ArithmeticOp::AddrOf:
		{
			return Result_t(this->expr->codegen(cgi).result.second, 0);
		}

		default:
			error("this, (%s:%s:%d) -> Internal check failed: invalid unary operator", __FILE__, __PRETTY_FUNCTION__, __LINE__);
			return Result_t(0, 0);
	}
}



Result_t BinOp::codegen(CodegenInstance* cgi)
{
	assert(this->left && this->right);
	ValPtr_t valptr = this->left->codegen(cgi).result;

	llvm::Value* lhs;
	llvm::Value* rhs;

	if(this->op == ArithmeticOp::Assign
		|| this->op == ArithmeticOp::PlusEquals			|| this->op == ArithmeticOp::MinusEquals
		|| this->op == ArithmeticOp::MultiplyEquals		|| this->op == ArithmeticOp::DivideEquals
		|| this->op == ArithmeticOp::ModEquals			|| this->op == ArithmeticOp::ShiftLeftEquals
		|| this->op == ArithmeticOp::ShiftRightEquals	|| this->op == ArithmeticOp::BitwiseAndEquals
		|| this->op == ArithmeticOp::BitwiseOrEquals	|| this->op == ArithmeticOp::BitwiseXorEquals)
	{
		this->right = cgi->autoCastType(this->left, this->right);

		lhs = valptr.first;
		rhs = this->right->codegen(cgi).result.first;

		VarRef* v = nullptr;
		UnaryOp* uo = nullptr;
		ArrayIndex* ai = nullptr;

		llvm::Value* varptr = 0;
		if((v = dynamic_cast<VarRef*>(this->left)))
		{
			if(!rhs)
				error(this, "(%s:%s:%d) -> Internal check failed: invalid RHS for assignment", __FILE__, __PRETTY_FUNCTION__, __LINE__);

			varptr = cgi->getSymTab()[v->name].first;
			if(!varptr)
				error(this, "Unknown identifier (var) '%s'", v->name.c_str());

			if(lhs->getType() != rhs->getType())
			{
				// ensure we can always store 0 to pointers without a cast
				Number* n = 0;
				if(rhs->getType()->isIntegerTy() && (n = dynamic_cast<Number*>(this->right)) && n->ival == 0)
				{
					rhs = llvm::Constant::getNullValue(varptr->getType()->getPointerElementType());
				}
				else if(lhs->getType()->isStructTy())
				{
					TypePair_t* tp = cgi->getType(lhs->getType()->getStructName());
					if(!tp)
						error(this, "Invalid type");

					// struct will handle all the weird operators by checking overloaded-ness
					// we only need to handle for arithmetic types
					return cgi->callOperatorOnStruct(tp, valptr.second, this->op, rhs);
				}
				else
				{
					error(this, "Cannot assign different types '%s' and '%s'", cgi->getReadableType(lhs->getType()).c_str(), cgi->getReadableType(rhs->getType()).c_str());
				}
			}
		}
		else if((dynamic_cast<MemberAccess*>(this->left))
			|| ((uo = dynamic_cast<UnaryOp*>(this->left)) && uo->op == ArithmeticOp::Deref)
			|| (ai = dynamic_cast<ArrayIndex*>(this->left)))
		{
			// we know that the ptr lives in the second element
			// so, use it

			varptr = valptr.second;
			assert(varptr);
			assert(rhs);

			// make sure the left side is a pointer
			if(!varptr->getType()->isPointerTy())
				error(this, "Expression (type '%s' = '%s') is not assignable.", cgi->getReadableType(varptr->getType()).c_str(), cgi->getReadableType(rhs->getType()).c_str());

			// redo the number casting
			if(rhs->getType()->isIntegerTy() && lhs->getType()->isIntegerTy())
				rhs = cgi->mainBuilder.CreateIntCast(rhs, varptr->getType()->getPointerElementType(), false);

			else if(rhs->getType()->isIntegerTy() && lhs->getType()->isPointerTy())
				rhs = cgi->mainBuilder.CreateIntToPtr(rhs, lhs->getType());
		}
		else
		{
			error(this, "Left-hand side of assignment must be assignable");
		}



		// do it all together now
		if(this->op == ArithmeticOp::Assign)
		{
			cgi->mainBuilder.CreateStore(rhs, varptr);
			return Result_t(rhs, varptr);
		}
		else
		{
			// get the llvm op
			llvm::Instruction::BinaryOps lop = cgi->getBinaryOperator(this->op, cgi->isSignedType(this->left) || cgi->isSignedType(this->right));

			llvm::Value* newrhs = cgi->mainBuilder.CreateBinOp(lop, lhs, rhs);
			cgi->mainBuilder.CreateStore(newrhs, varptr);
			return Result_t(newrhs, varptr);
		}
	}
	else if(this->op == ArithmeticOp::Cast)
	{
		lhs = valptr.first;

		// right hand side probably got interpreted as a varref
		CastedType* ct = nullptr;
		assert(ct = dynamic_cast<CastedType*>(this->right));

		llvm::Type* rtype = cgi->getLlvmType(ct);
		if(!rtype)
		{
			TypePair_t* tp = cgi->getType(ct->name);
			if(!tp)
				error(this, "Unknown type '%s'", ct->name.c_str());

			rtype = tp->first;
		}


		// todo: cleanup?
		assert(rtype);
		if(lhs->getType() == rtype)
			return Result_t(lhs, 0);

		if(lhs->getType()->isIntegerTy() && rtype->isIntegerTy())
			return Result_t(cgi->mainBuilder.CreateIntCast(lhs, rtype, cgi->isSignedType(this->left)), 0);

		else if(lhs->getType()->isFloatTy() && rtype->isFloatTy())
			return Result_t(cgi->mainBuilder.CreateFPCast(lhs, rtype), 0);

		else if(lhs->getType()->isPointerTy() && rtype->isPointerTy())
			return Result_t(cgi->mainBuilder.CreatePointerCast(lhs, rtype), 0);

		else if(lhs->getType()->isPointerTy() && rtype->isIntegerTy())
			return Result_t(cgi->mainBuilder.CreatePtrToInt(lhs, rtype), 0);

		else if(lhs->getType()->isIntegerTy() && rtype->isPointerTy())
			return Result_t(cgi->mainBuilder.CreateIntToPtr(lhs, rtype), 0);

		else
			return Result_t(cgi->mainBuilder.CreateBitCast(lhs, rtype), 0);
	}


	// else case.
	// no point being explicit about this and wasting indentation

	this->right = cgi->autoCastType(this->left, this->right);

	lhs = valptr.first;
	llvm::Value* lhsptr = valptr.second;
	auto r = this->right->codegen(cgi).result;

	rhs = r.first;
	llvm::Value* rhsptr = r.second;


	// if adding integer to pointer
	if(lhs->getType()->isPointerTy() && rhs->getType()->isIntegerTy()
		&& (this->op == ArithmeticOp::Add || this->op == ArithmeticOp::Subtract))
	{
		return cgi->doPointerArithmetic(this->op, lhs, lhsptr, rhs);
	}



	// allow to cascade down if the above fails
	if(lhs->getType()->isIntegerTy() && rhs->getType()->isIntegerTy())
	{
		llvm::Instruction::BinaryOps lop = cgi->getBinaryOperator(this->op,
			cgi->isSignedType(this->left) || cgi->isSignedType(this->right));

		if(lop != (llvm::Instruction::BinaryOps) 0)
			return Result_t(cgi->mainBuilder.CreateBinOp(lop, lhs, rhs), 0);

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
				llvm::Value* res = cgi->mainBuilder.CreateTrunc(lhs, llvm::Type::getInt1Ty(cgi->getContext()));
				llvm::Value* ret = nullptr;

				llvm::BasicBlock* entry = cgi->mainBuilder.GetInsertBlock();
				llvm::BasicBlock* lb = llvm::BasicBlock::Create(cgi->getContext(), "leftbl", func);
				llvm::BasicBlock* rb = llvm::BasicBlock::Create(cgi->getContext(), "rightbl", func);
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

				return Result_t(this->phi, 0);
			}

			default:
				// should not be reached
				error("what?!");
				return Result_t(0, 0);
		}
	}
	else if(cgi->isBuiltinType(this->left) && cgi->isBuiltinType(this->right))
	{
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

			default:						error(this, "Unsupported operator."); return Result_t(0, 0);
		}
	}
	else if(lhs->getType()->isStructTy())
	{
		TypePair_t* p = cgi->getType(lhs->getType()->getStructName());
		if(!p)
			error(this, "Invalid type");

		return cgi->callOperatorOnStruct(p, valptr.second, op, rhsptr);
	}
	else
	{
		error(this, "Unsupported operator on type");
		return Result_t(0, 0);
	}
}





