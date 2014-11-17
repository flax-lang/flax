// ExprCodeGen.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "../include/ast.h"
#include "../include/codegen.h"
#include "../include/llvm_all.h"

using namespace Ast;
using namespace Codegen;

llvm::Value* Number::codeGen()
{
	// check builtin type
	if(this->varType <= VarType::Uint64)
		return llvm::ConstantInt::get(getContext(), llvm::APInt(pow(2, (int) this->varType % 4) * 8, this->ival, this->varType > VarType::Int64));

	else if(this->type == "Float32" || this->type == "Float64")
		return llvm::ConstantFP::get(getContext(), llvm::APFloat(this->dval));

	error("(%s:%s:%d) -> Internal check failed: invalid number", __FILE__, __PRETTY_FUNCTION__, __LINE__);
	return nullptr;
}

llvm::Value* VarRef::codeGen()
{
	llvm::Value* val = getSymInst(this->name);
	if(!val)
		error("Unknown variable name '%s'", this->name.c_str());

	return mainBuilder.CreateLoad(val, this->name);
}

llvm::Value* VarDecl::codeGen()
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
		val = this->initVal->codeGen();
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

			printf("init call: [%s]\n", getReadableType(ai->getType()).c_str());
			val = mainBuilder.CreateCall(str->initFunc, ai);
		}
		else
		{
			error("Unknown type encountered");
		}
	}

	mainBuilder.CreateStore(val, ai);
	return val;
}

llvm::Value* Return::codeGen()
{
	return mainBuilder.CreateRet(this->val->codeGen());
}

llvm::Value* UnaryOp::codeGen()
{
	assert(this->expr);
	assert(this->op == ArithmeticOp::LogicalNot || this->op == ArithmeticOp::Plus || this->op == ArithmeticOp::Minus);

	switch(this->op)
	{
		case ArithmeticOp::LogicalNot:
			return mainBuilder.CreateNot(this->expr->codeGen());

		case ArithmeticOp::Minus:
			return mainBuilder.CreateNeg(this->expr->codeGen());

		case ArithmeticOp::Plus:
			return this->expr->codeGen();

		default:
			error("(%s:%s:%d) -> Internal check failed: invalid unary operator", __FILE__, __PRETTY_FUNCTION__, __LINE__);
			return nullptr;
	}
}



llvm::Value* BinOp::codeGen()
{
	llvm::Value* lhs;
	llvm::Value* rhs;

	this->right = autoCastType(this->left, this->right);
	lhs = this->left->codeGen();
	rhs = this->right->codeGen();

	if(this->op == ArithmeticOp::Assign)
	{
		VarRef* v;
		if(!(v = dynamic_cast<VarRef*>(this->left)))
			error("Left-hand side of assignment must be assignable");

		if(!rhs)
			error("(%s:%s:%d) -> Internal check failed: invalid RHS for assignment", __FILE__, __PRETTY_FUNCTION__, __LINE__);

		llvm::Value* var = getSymTab()[v->name].first;
		if(!var)
			error("Unknown identifier (var) '%s'", v->name.c_str());

		mainBuilder.CreateStore(rhs, var);
		return rhs;
	}


	// if both ops are integer values
	if(isIntegerType(this->left) && isIntegerType(this->right))
	{
		switch(this->op)
		{
			case ArithmeticOp::Add:											return mainBuilder.CreateAdd(lhs, rhs);
			case ArithmeticOp::Subtract:									return mainBuilder.CreateSub(lhs, rhs);
			case ArithmeticOp::Multiply:									return mainBuilder.CreateMul(lhs, rhs);
			case ArithmeticOp::ShiftLeft:									return mainBuilder.CreateShl(lhs, rhs);
			case ArithmeticOp::Divide:
				if(isSignedType(this->left) || isSignedType(this->right))	return mainBuilder.CreateSDiv(lhs, rhs);
				else 														return mainBuilder.CreateUDiv(lhs, rhs);
			case ArithmeticOp::Modulo:
				if(isSignedType(this->left) || isSignedType(this->right))	return mainBuilder.CreateSRem(lhs, rhs);
				else 														return mainBuilder.CreateURem(lhs, rhs);
			case ArithmeticOp::ShiftRight:
				if(isSignedType(this->left))								return mainBuilder.CreateAShr(lhs, rhs);
				else 														return mainBuilder.CreateLShr(lhs, rhs);

			// comparisons
			case ArithmeticOp::CmpEq:										return mainBuilder.CreateICmpEQ(lhs, rhs, "cmptmp");
			case ArithmeticOp::CmpNEq:										return mainBuilder.CreateICmpNE(lhs, rhs, "cmptmp");
			case ArithmeticOp::CmpLT:
				if(isSignedType(this->left) || isSignedType(this->right))	return mainBuilder.CreateICmpSLT(lhs, rhs, "cmptmp");
				else 														return mainBuilder.CreateICmpULT(lhs, rhs, "cmptmp");
			case ArithmeticOp::CmpGT:
				if(isSignedType(this->left) || isSignedType(this->right))	return mainBuilder.CreateICmpSGT(lhs, rhs, "cmptmp");
				else 														return mainBuilder.CreateICmpUGT(lhs, rhs, "cmptmp");
			case ArithmeticOp::CmpLEq:
				if(isSignedType(this->left) || isSignedType(this->right))	return mainBuilder.CreateICmpSLE(lhs, rhs, "cmptmp");
				else 														return mainBuilder.CreateICmpULE(lhs, rhs, "cmptmp");
			case ArithmeticOp::CmpGEq:
				if(isSignedType(this->left) || isSignedType(this->right))	return mainBuilder.CreateICmpSGE(lhs, rhs, "cmptmp");
				else 														return mainBuilder.CreateICmpUGE(lhs, rhs, "cmptmp");

			default:
				// should not be reached
				error("what?!");
				return 0;
		}
	}
	else if(isBuiltinType(this->left) && isBuiltinType(this->right))
	{
		switch(this->op)
		{
			case ArithmeticOp::Add:			return mainBuilder.CreateFAdd(lhs, rhs);
			case ArithmeticOp::Subtract:	return mainBuilder.CreateFSub(lhs, rhs);
			case ArithmeticOp::Multiply:	return mainBuilder.CreateFMul(lhs, rhs);
			case ArithmeticOp::Divide:		return mainBuilder.CreateFDiv(lhs, rhs);

			// comparisons
			case ArithmeticOp::CmpEq:		return mainBuilder.CreateFCmpOEQ(lhs, rhs);
			case ArithmeticOp::CmpNEq:		return mainBuilder.CreateFCmpONE(lhs, rhs);
			case ArithmeticOp::CmpLT:		return mainBuilder.CreateFCmpOLT(lhs, rhs);
			case ArithmeticOp::CmpGT:		return mainBuilder.CreateFCmpOGT(lhs, rhs);
			case ArithmeticOp::CmpLEq:		return mainBuilder.CreateFCmpOLE(lhs, rhs);
			case ArithmeticOp::CmpGEq:		return mainBuilder.CreateFCmpOGE(lhs, rhs);

			default:						error("Unsupported operator."); return nullptr;
		}
	}
	else
	{
		error("Unsupported operator on type");
		return nullptr;
	}
}





