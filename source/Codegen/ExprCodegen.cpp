// ExprCodeGen.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "../include/ast.h"
#include "../include/codegen.h"
#include "../include/llvm_all.h"

using namespace Ast;
using namespace Codegen;


ValPtr_p Return::codegen(CodegenInstance* cgi)
{
	auto ret = this->val->codegen(cgi);
	return ValPtr_p(cgi->mainBuilder.CreateRet(ret.first), ret.second);
}

ValPtr_p UnaryOp::codegen(CodegenInstance* cgi)
{
	assert(this->expr);
	switch(this->op)
	{
		case ArithmeticOp::LogicalNot:
			return ValPtr_p(cgi->mainBuilder.CreateNot(this->expr->codegen(cgi).first), 0);

		case ArithmeticOp::Minus:
			return ValPtr_p(cgi->mainBuilder.CreateNeg(this->expr->codegen(cgi).first), 0);

		case ArithmeticOp::Plus:
			return this->expr->codegen(cgi);

		case ArithmeticOp::Deref:
		{
			ValPtr_p vp = this->expr->codegen(cgi);
			return ValPtr_p(cgi->mainBuilder.CreateLoad(vp.first), vp.first);
		}

		case ArithmeticOp::AddrOf:
		{
			return ValPtr_p(this->expr->codegen(cgi).second, 0);
		}

		default:
			error("(%s:%s:%d) -> Internal check failed: invalid unary operator", __FILE__, __PRETTY_FUNCTION__, __LINE__);
			return ValPtr_p(0, 0);
	}
}



ValPtr_p BinOp::codegen(CodegenInstance* cgi)
{
	assert(this->left && this->right);
	this->right = cgi->autoCastType(this->left, this->right);

	ValPtr_p valptr = this->left->codegen(cgi);

	llvm::Value* lhs;
	llvm::Value* rhs;

	if(this->op == ArithmeticOp::Assign)
	{
		lhs = valptr.first;
		rhs = this->right->codegen(cgi).first;

		VarRef* v = nullptr;
		UnaryOp* uo = nullptr;
		ArrayIndex* ai = nullptr;
		if((v = dynamic_cast<VarRef*>(this->left)))
		{
			if(!rhs)
				error("(%s:%s:%d) -> Internal check failed: invalid RHS for assignment", __FILE__, __PRETTY_FUNCTION__, __LINE__);

			llvm::Value* var = cgi->getSymTab()[v->name].first;
			if(!var)
				error(this, "Unknown identifier (var) '%s'", v->name.c_str());

			if(lhs->getType() != rhs->getType())
			{
				if(lhs->getType()->isStructTy())
				{
					TypePair_t* tp = cgi->getType(lhs->getType()->getStructName());
					if(!tp)
						error(this, "Invalid type");

					return cgi->callOperatorOnStruct(tp, valptr.second, ArithmeticOp::Assign, rhs);
				}
				else
				{
					error(this, "Cannot assign different types '%s' and '%s'", cgi->getReadableType(lhs->getType()).c_str(), cgi->getReadableType(rhs->getType()).c_str());
				}
			}

			cgi->mainBuilder.CreateStore(rhs, var);
			return ValPtr_p(rhs, var);
		}
		else if((dynamic_cast<MemberAccess*>(this->left))
			|| ((uo = dynamic_cast<UnaryOp*>(this->left)) && uo->op == ArithmeticOp::Deref)
			|| (ai = dynamic_cast<ArrayIndex*>(this->left)))
		{
			// we know that the ptr lives in the second element
			// so, use it

			llvm::Value* ptr = valptr.second;
			assert(ptr);
			assert(rhs);

			// make sure the left side is a pointer
			if(!ptr->getType()->isPointerTy())
				error(this, "Expression (type '%s' = '%s') is not assignable.", cgi->getReadableType(ptr->getType()).c_str(), cgi->getReadableType(rhs->getType()).c_str());

			// redo the number casting
			if(rhs->getType()->isIntegerTy() && lhs->getType()->isIntegerTy())
				rhs = cgi->mainBuilder.CreateIntCast(rhs, ptr->getType()->getPointerElementType(), false);

			else if(rhs->getType()->isIntegerTy() && lhs->getType()->isPointerTy())
				rhs = cgi->mainBuilder.CreateIntToPtr(rhs, lhs->getType());

			// assign it
			cgi->mainBuilder.CreateStore(rhs, ptr);
			return ValPtr_p(rhs, ptr);
		}
		else
		{
			error("Left-hand side of assignment must be assignable");
		}
	}
	else if(this->op == ArithmeticOp::Cast)
	{
		lhs = valptr.first;

		// right hand side probably got interpreted as a varref
		VarRef* vr = nullptr;
		assert(vr = dynamic_cast<VarRef*>(this->right));

		llvm::Type* rtype;
		VarType vt = Parser::determineVarType(vr->name);
		if(vt != VarType::UserDefined)
		{
			rtype = cgi->getLlvmTypeOfBuiltin(vt);
		}
		else
		{
			TypePair_t* tp = cgi->getType(vr->name);
			if(!tp)
				error(this, "(ExprCodegen.cpp:~147): Unknown type '%s'", vr->name.c_str());

			rtype = tp->first;
		}

		// todo: cleanup?
		assert(rtype);
		if(lhs->getType() == rtype)
			return ValPtr_p(lhs, 0);

		if(lhs->getType()->isIntegerTy() && rtype->isIntegerTy())
			return ValPtr_p(cgi->mainBuilder.CreateIntCast(lhs, rtype, cgi->isSignedType(this->left)), 0);

		else if(lhs->getType()->isFloatTy() && rtype->isFloatTy())
			return ValPtr_p(cgi->mainBuilder.CreateFPCast(lhs, rtype), 0);

		else if(lhs->getType()->isPointerTy() && rtype->isPointerTy())
			return ValPtr_p(cgi->mainBuilder.CreatePointerCast(lhs, rtype), 0);

		else if(lhs->getType()->isPointerTy() && rtype->isIntegerTy())
			return ValPtr_p(cgi->mainBuilder.CreatePtrToInt(lhs, rtype), 0);

		else if(lhs->getType()->isIntegerTy() && rtype->isPointerTy())
			return ValPtr_p(cgi->mainBuilder.CreateIntToPtr(lhs, rtype), 0);

		else
			return ValPtr_p(cgi->mainBuilder.CreateBitCast(lhs, rtype), 0);
	}




	lhs = valptr.first;
	llvm::Value* rhsptr = nullptr;
	auto r = this->right->codegen(cgi);

	rhs = r.first;
	rhsptr = r.second;

	if(lhs->getType()->isIntegerTy() && rhs->getType()->isIntegerTy())
	{
		switch(this->op)
		{
			case ArithmeticOp::Add:					return ValPtr_p(cgi->mainBuilder.CreateAdd(lhs, rhs), 0);
			case ArithmeticOp::Subtract:			return ValPtr_p(cgi->mainBuilder.CreateSub(lhs, rhs), 0);
			case ArithmeticOp::Multiply:			return ValPtr_p(cgi->mainBuilder.CreateMul(lhs, rhs), 0);
			case ArithmeticOp::ShiftLeft:			return ValPtr_p(cgi->mainBuilder.CreateShl(lhs, rhs), 0);
			case ArithmeticOp::Divide:
				if(cgi->isSignedType(this->left) || cgi->isSignedType(this->right))
					return ValPtr_p(cgi->mainBuilder.CreateSDiv(lhs, rhs), 0);
				else
					return ValPtr_p(cgi->mainBuilder.CreateUDiv(lhs, rhs), 0);


			case ArithmeticOp::Modulo:
				if(cgi->isSignedType(this->left) || cgi->isSignedType(this->right))
					return ValPtr_p(cgi->mainBuilder.CreateSRem(lhs, rhs), 0);
				else
					return ValPtr_p(cgi->mainBuilder.CreateURem(lhs, rhs), 0);


			case ArithmeticOp::ShiftRight:
				if(cgi->isSignedType(this->left))	return ValPtr_p(cgi->mainBuilder.CreateAShr(lhs, rhs), 0);
				else 								return ValPtr_p(cgi->mainBuilder.CreateLShr(lhs, rhs), 0);

			// comparisons
			case ArithmeticOp::CmpEq:		return ValPtr_p(cgi->mainBuilder.CreateICmpEQ(lhs, rhs), 0);
			case ArithmeticOp::CmpNEq:		return ValPtr_p(cgi->mainBuilder.CreateICmpNE(lhs, rhs), 0);


			case ArithmeticOp::CmpLT:
				if(cgi->isSignedType(this->left) || cgi->isSignedType(this->right))
					return ValPtr_p(cgi->mainBuilder.CreateICmpSLT(lhs, rhs), 0);
				else
					return ValPtr_p(cgi->mainBuilder.CreateICmpULT(lhs, rhs), 0);



			case ArithmeticOp::CmpGT:
				if(cgi->isSignedType(this->left) || cgi->isSignedType(this->right))
					return ValPtr_p(cgi->mainBuilder.CreateICmpSGT(lhs, rhs), 0);
				else
					return ValPtr_p(cgi->mainBuilder.CreateICmpUGT(lhs, rhs), 0);


			case ArithmeticOp::CmpLEq:
				if(cgi->isSignedType(this->left) || cgi->isSignedType(this->right))
					return ValPtr_p(cgi->mainBuilder.CreateICmpSLE(lhs, rhs), 0);
				else
					return ValPtr_p(cgi->mainBuilder.CreateICmpULE(lhs, rhs), 0);



			case ArithmeticOp::CmpGEq:
				if(cgi->isSignedType(this->left) || cgi->isSignedType(this->right))
					return ValPtr_p(cgi->mainBuilder.CreateICmpSGE(lhs, rhs), 0);
				else
					return ValPtr_p(cgi->mainBuilder.CreateICmpUGE(lhs, rhs), 0);



			case ArithmeticOp::BitwiseAnd:		return ValPtr_p(cgi->mainBuilder.CreateAnd(lhs, rhs), 0);
			case ArithmeticOp::BitwiseOr:		return ValPtr_p(cgi->mainBuilder.CreateOr(lhs, rhs), 0);


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

				return ValPtr_p(this->phi, 0);
			}


			default:
				// should not be reached
				error("what?!");
				return ValPtr_p(0, 0);
		}
	}
	else if(cgi->isBuiltinType(this->left) && cgi->isBuiltinType(this->right))
	{
		// then they're floats.
		switch(this->op)
		{
			case ArithmeticOp::Add:			return ValPtr_p(cgi->mainBuilder.CreateFAdd(lhs, rhs), 0);
			case ArithmeticOp::Subtract:	return ValPtr_p(cgi->mainBuilder.CreateFSub(lhs, rhs), 0);
			case ArithmeticOp::Multiply:	return ValPtr_p(cgi->mainBuilder.CreateFMul(lhs, rhs), 0);
			case ArithmeticOp::Divide:		return ValPtr_p(cgi->mainBuilder.CreateFDiv(lhs, rhs), 0);

			// comparisons
			case ArithmeticOp::CmpEq:		return ValPtr_p(cgi->mainBuilder.CreateFCmpOEQ(lhs, rhs), 0);
			case ArithmeticOp::CmpNEq:		return ValPtr_p(cgi->mainBuilder.CreateFCmpONE(lhs, rhs), 0);
			case ArithmeticOp::CmpLT:		return ValPtr_p(cgi->mainBuilder.CreateFCmpOLT(lhs, rhs), 0);
			case ArithmeticOp::CmpGT:		return ValPtr_p(cgi->mainBuilder.CreateFCmpOGT(lhs, rhs), 0);
			case ArithmeticOp::CmpLEq:		return ValPtr_p(cgi->mainBuilder.CreateFCmpOLE(lhs, rhs), 0);
			case ArithmeticOp::CmpGEq:		return ValPtr_p(cgi->mainBuilder.CreateFCmpOGE(lhs, rhs), 0);

			default:						error("Unsupported operator."); return ValPtr_p(0, 0);
		}
	}
	else if(lhs->getType()->isStructTy())
	{
		TypePair_t* p = cgi->getType(lhs->getType()->getStructName());
		if(!p)
			error("Invalid type");

		return cgi->callOperatorOnStruct(p, valptr.second, op, rhsptr);
	}
	else
	{
		error("Unsupported operator on type");
		return ValPtr_p(0, 0);
	}
}





