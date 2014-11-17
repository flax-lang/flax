// ExprCodeGen.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "../include/ast.h"
#include "../include/codegen.h"
#include "../include/llvm_all.h"

using namespace Ast;
using namespace Codegen;

ValPtr_p Number::codeGen()
{
	// check builtin type
	if(this->varType <= VarType::Uint64)
		return ValPtr_p(llvm::ConstantInt::get(getContext(), llvm::APInt(pow(2, (int) this->varType % 4) * 8, this->ival, this->varType > VarType::Int64)), 0);

	else if(this->type == "Float32" || this->type == "Float64")
		return ValPtr_p(llvm::ConstantFP::get(getContext(), llvm::APFloat(this->dval)), 0);

	error("(%s:%s:%d) -> Internal check failed: invalid number", __FILE__, __PRETTY_FUNCTION__, __LINE__);
	return ValPtr_p(0, 0);
}

ValPtr_p VarRef::codeGen()
{
	llvm::Value* val = getSymInst(this->name);
	if(!val)
		error("Unknown variable name '%s'", this->name.c_str());

	return ValPtr_p(mainBuilder.CreateLoad(val, this->name), val);
}

ValPtr_p VarDecl::codeGen()
{
	if(isDuplicateSymbol(this->name))
		error("Redefining duplicate symbol '%s'", this->name.c_str());

	llvm::Function* func = mainBuilder.GetInsertBlock()->getParent();
	llvm::Value* val = nullptr;

	llvm::AllocaInst* ai = allocateInstanceInBlock(func, this);
	getSymTab()[this->name] = std::pair<llvm::AllocaInst*, VarDecl*>(ai, this);

	if(this->initVal)
	{
		this->initVal = autoCastType(this, this->initVal);
		val = this->initVal->codeGen().first;
	}
	else if(isBuiltinType(this))
	{
		val = getDefaultValue(this);
	}
	else
	{
		// get our type
		TypePair_t* pair = getType(this->type);
		if(!pair)
			error("Invalid type");

		if(pair->first->isStructTy())
		{
			assert(pair->second.second == ExprType::Struct);
			assert(pair->second.first);

			Struct* str = nullptr;
			assert((str = dynamic_cast<Struct*>(pair->second.first)));

			val = mainBuilder.CreateCall(str->initFunc, ai);

			// if it's not a pointer, we need to dereference.
			if(!isPtr(this))
				val = mainBuilder.CreateLoad(val, "derefPtr");
		}
		else
		{
			error("Unknown type encountered");
		}
	}

	mainBuilder.CreateStore(val, ai);
	return ValPtr_p(val, ai);
}

ValPtr_p Return::codeGen()
{
	return ValPtr_p(mainBuilder.CreateRet(this->val->codeGen().first), 0);
}

ValPtr_p UnaryOp::codeGen()
{
	assert(this->expr);
	switch(this->op)
	{
		case ArithmeticOp::LogicalNot:
			return ValPtr_p(mainBuilder.CreateNot(this->expr->codeGen().first), 0);

		case ArithmeticOp::Minus:
			return ValPtr_p(mainBuilder.CreateNeg(this->expr->codeGen().first), 0);

		case ArithmeticOp::Plus:
			return this->expr->codeGen();

		case ArithmeticOp::Deref:
		{
			ValPtr_p vp = this->expr->codeGen();
			return ValPtr_p(mainBuilder.CreateLoad(vp.first), vp.first);
		}

		case ArithmeticOp::AddrOf:
		{
			VarRef* vr = nullptr;
			if((vr = dynamic_cast<VarRef*>(this->expr)))
			{
				return ValPtr_p(getSymInst(vr->name), 0);
			}
			else
			{
				error("Cannot take the address of that");
			}
		}

		default:
			error("(%s:%s:%d) -> Internal check failed: invalid unary operator", __FILE__, __PRETTY_FUNCTION__, __LINE__);
			return ValPtr_p(0, 0);
	}
}



ValPtr_p BinOp::codeGen()
{
	this->right = autoCastType(this->left, this->right);

	ValPtr_p valptr = this->left->codeGen();

	llvm::Value* lhs = valptr.first;
	llvm::Value* rhs = this->right->codeGen().first;

	if(this->op == ArithmeticOp::Assign)
	{
		VarRef* v = nullptr;
		UnaryOp* uo = nullptr;
		if((v = dynamic_cast<VarRef*>(this->left)))
		{
			if(!rhs)
				error("(%s:%s:%d) -> Internal check failed: invalid RHS for assignment", __FILE__, __PRETTY_FUNCTION__, __LINE__);

			llvm::Value* var = getSymTab()[v->name].first;
			if(!var)
				error("Unknown identifier (var) '%s'", v->name.c_str());

			mainBuilder.CreateStore(rhs, var);
			return ValPtr_p(rhs, var);
		}
		else if((dynamic_cast<MemberAccess*>(this->left)) || ((uo = dynamic_cast<UnaryOp*>(this->left)) && uo->op == ArithmeticOp::Deref))
		{
			// we know that the ptr lives in the second element
			// so, use it

			llvm::Value* ptr = valptr.second;
			assert(ptr);

			// make sure the left side is a pointer
			if(!ptr->getType()->isPointerTy())
				error("Expression (type '%s' = '%s') is not assignable.", getReadableType(ptr->getType()).c_str(), getReadableType(rhs->getType()).c_str());

			// dereference it
			mainBuilder.CreateStore(rhs, ptr);
			return ValPtr_p(rhs, ptr);
		}
		else
		{
			error("Left-hand side of assignment must be assignable");
		}
	}

	// if both ops are integer values
	if(isIntegerType(this->left) && isIntegerType(this->right))
	{
		switch(this->op)
		{
			case ArithmeticOp::Add:											return ValPtr_p(mainBuilder.CreateAdd(lhs, rhs), 0);
			case ArithmeticOp::Subtract:									return ValPtr_p(mainBuilder.CreateSub(lhs, rhs), 0);
			case ArithmeticOp::Multiply:									return ValPtr_p(mainBuilder.CreateMul(lhs, rhs), 0);
			case ArithmeticOp::ShiftLeft:									return ValPtr_p(mainBuilder.CreateShl(lhs, rhs), 0);
			case ArithmeticOp::Divide:
				if(isSignedType(this->left) || isSignedType(this->right))	return ValPtr_p(mainBuilder.CreateSDiv(lhs, rhs), 0);
				else 														return ValPtr_p(mainBuilder.CreateUDiv(lhs, rhs), 0);
			case ArithmeticOp::Modulo:
				if(isSignedType(this->left) || isSignedType(this->right))	return ValPtr_p(mainBuilder.CreateSRem(lhs, rhs), 0);
				else 														return ValPtr_p(mainBuilder.CreateURem(lhs, rhs), 0);
			case ArithmeticOp::ShiftRight:
				if(isSignedType(this->left))								return ValPtr_p(mainBuilder.CreateAShr(lhs, rhs), 0);
				else 														return ValPtr_p(mainBuilder.CreateLShr(lhs, rhs), 0);

			// comparisons
			case ArithmeticOp::CmpEq:										return ValPtr_p(mainBuilder.CreateICmpEQ(lhs, rhs, "cmptmp"), 0);
			case ArithmeticOp::CmpNEq:										return ValPtr_p(mainBuilder.CreateICmpNE(lhs, rhs, "cmptmp"), 0);
			case ArithmeticOp::CmpLT:
				if(isSignedType(this->left) || isSignedType(this->right))	return ValPtr_p(mainBuilder.CreateICmpSLT(lhs, rhs, "cmptmp"), 0);
				else 														return ValPtr_p(mainBuilder.CreateICmpULT(lhs, rhs, "cmptmp"), 0);
			case ArithmeticOp::CmpGT:
				if(isSignedType(this->left) || isSignedType(this->right))	return ValPtr_p(mainBuilder.CreateICmpSGT(lhs, rhs, "cmptmp"), 0);
				else 														return ValPtr_p(mainBuilder.CreateICmpUGT(lhs, rhs, "cmptmp"), 0);
			case ArithmeticOp::CmpLEq:
				if(isSignedType(this->left) || isSignedType(this->right))	return ValPtr_p(mainBuilder.CreateICmpSLE(lhs, rhs, "cmptmp"), 0);
				else 														return ValPtr_p(mainBuilder.CreateICmpULE(lhs, rhs, "cmptmp"), 0);
			case ArithmeticOp::CmpGEq:
				if(isSignedType(this->left) || isSignedType(this->right))	return ValPtr_p(mainBuilder.CreateICmpSGE(lhs, rhs, "cmptmp"), 0);
				else 														return ValPtr_p(mainBuilder.CreateICmpUGE(lhs, rhs, "cmptmp"), 0);

			default:
				// should not be reached
				error("what?!");
				return ValPtr_p(0, 0);
		}
	}
	else if(isBuiltinType(this->left) && isBuiltinType(this->right))
	{
		switch(this->op)
		{
			case ArithmeticOp::Add:			return ValPtr_p(mainBuilder.CreateFAdd(lhs, rhs), 0);
			case ArithmeticOp::Subtract:	return ValPtr_p(mainBuilder.CreateFSub(lhs, rhs), 0);
			case ArithmeticOp::Multiply:	return ValPtr_p(mainBuilder.CreateFMul(lhs, rhs), 0);
			case ArithmeticOp::Divide:		return ValPtr_p(mainBuilder.CreateFDiv(lhs, rhs), 0);

			// comparisons
			case ArithmeticOp::CmpEq:		return ValPtr_p(mainBuilder.CreateFCmpOEQ(lhs, rhs), 0);
			case ArithmeticOp::CmpNEq:		return ValPtr_p(mainBuilder.CreateFCmpONE(lhs, rhs), 0);
			case ArithmeticOp::CmpLT:		return ValPtr_p(mainBuilder.CreateFCmpOLT(lhs, rhs), 0);
			case ArithmeticOp::CmpGT:		return ValPtr_p(mainBuilder.CreateFCmpOGT(lhs, rhs), 0);
			case ArithmeticOp::CmpLEq:		return ValPtr_p(mainBuilder.CreateFCmpOLE(lhs, rhs), 0);
			case ArithmeticOp::CmpGEq:		return ValPtr_p(mainBuilder.CreateFCmpOGE(lhs, rhs), 0);

			default:						error("Unsupported operator."); return ValPtr_p(0, 0);
		}
	}
	else
	{
		error("Unsupported operator on type");
		return ValPtr_p(0, 0);
	}
}





