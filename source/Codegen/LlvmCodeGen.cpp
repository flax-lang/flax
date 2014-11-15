// LlvmCodeGen.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include <map>
#include <vector>
#include <stdint.h>
#include "../include/ast.h"

#include "llvm/Analysis/Passes.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/PassManager.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Transforms/Scalar.h"

using namespace Ast;

static llvm::Module* mainModule = new llvm::Module("mainModule", llvm::getGlobalContext());
static llvm::IRBuilder<> mainBuilder = llvm::IRBuilder<>(llvm::getGlobalContext());
static llvm::FunctionPassManager* Fpm;
static std::map<std::string, llvm::Value*> symbolTable;



static void error(const char* msg, ...)
{
	va_list ap;
	va_start(ap, msg);

	char* alloc = nullptr;
	vasprintf(&alloc, msg, ap);

	fprintf(stderr, "Error: %s\n\n", alloc);

	va_end(ap);
	exit(1);
}

static bool isBuiltinType(Expr* e)
{
	return e->varType <= VarType::Bool || e->varType == VarType::Float32 || e->varType == VarType::Float64 || e->varType == VarType::Void;
}

static bool isIntegerType(Expr* e)		{ return e->varType <= VarType::Uint64; }
static bool isSignedType(Expr* e)		{ return e->varType <= VarType::Int64; }
static llvm::Type* getLlvmType(VarType t)
{
	switch(t)
	{
		case VarType::Uint8:
		case VarType::Int8:		return llvm::Type::getInt8Ty(llvm::getGlobalContext());

		case VarType::Uint16:
		case VarType::Int16:	return llvm::Type::getInt16Ty(llvm::getGlobalContext());

		case VarType::Uint32:
		case VarType::Int32:	return llvm::Type::getInt32Ty(llvm::getGlobalContext());

		case VarType::Uint64:
		case VarType::Int64:	return llvm::Type::getInt64Ty(llvm::getGlobalContext());

		case VarType::Float32:	return llvm::Type::getFloatTy(llvm::getGlobalContext());
		case VarType::Float64:	return llvm::Type::getDoubleTy(llvm::getGlobalContext());

		case VarType::Void:		return llvm::Type::getVoidTy(llvm::getGlobalContext());

		default:
			error("User-defined types not yet supported (found %d)", t);
	}

	return nullptr;
}



llvm::Value* Number::codeGen()
{
	// check builtin type
	if(this->varType <= VarType::Uint64)
		return llvm::ConstantInt::get(llvm::getGlobalContext(), llvm::APInt(((int) this->varType % 4 + 1) * 8, this->ival, this->varType > VarType::Int64));

	else if(this->type == "Float32" || this->type == "Float64")
		return llvm::ConstantFP::get(llvm::getGlobalContext(), llvm::APFloat(this->dval));

	else
		return nullptr;
}

llvm::Value* Var::codeGen()
{
	llvm::Value* val = symbolTable[this->name];

	if(!val)
		error("Unknown variable name '%s'", this->name.c_str());

	return val;
}

llvm::Value* BinOp::codeGen()
{
	// neat
	llvm::Value* lhs = this->left->codeGen();
	llvm::Value* rhs = this->right->codeGen();

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

			case ArithmeticOp::Assign:										error("Assignment unsupported"); return nullptr;

			// comparisons
			case ArithmeticOp::CmpEq:										return mainBuilder.CreateICmpEQ(lhs, rhs);
			case ArithmeticOp::CmpNEq:										return mainBuilder.CreateICmpNE(lhs, rhs);
			case ArithmeticOp::CmpLT:
				if(isSignedType(this->left) || isSignedType(this->right))	return mainBuilder.CreateICmpSLT(lhs, rhs);
				else 														return mainBuilder.CreateICmpULT(lhs, rhs);
			case ArithmeticOp::CmpGT:
				if(isSignedType(this->left) || isSignedType(this->right))	return mainBuilder.CreateICmpSGT(lhs, rhs);
				else 														return mainBuilder.CreateICmpUGT(lhs, rhs);
			case ArithmeticOp::CmpLEq:
				if(isSignedType(this->left) || isSignedType(this->right))	return mainBuilder.CreateICmpSLE(lhs, rhs);
				else 														return mainBuilder.CreateICmpULE(lhs, rhs);
			case ArithmeticOp::CmpGEq:
				if(isSignedType(this->left) || isSignedType(this->right))	return mainBuilder.CreateICmpSGE(lhs, rhs);
				else 														return mainBuilder.CreateICmpUGE(lhs, rhs);
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


llvm::Value* FuncCall::codeGen()
{
	llvm::Function* target = mainModule->getFunction(this->name);
	if(target == 0)
		error("Unknown function '%s'", this->name.c_str());

	if(target->arg_size() != this->params.size())
		error("Expected %ld arguments, but got %ld arguments instead", target->arg_size(), this->params.size());

	std::vector<llvm::Value*> args;
	for(Expr* e : this->params)
	{
		args.push_back(e->codeGen());
		if(args.back() == nullptr)
			return 0;
	}

	return mainBuilder.CreateCall(target, args);
}

llvm::Value* FuncDecl::codeGen()
{
	std::vector<llvm::Type*> argtypes;
	for(Var* v : this->params)
		argtypes.push_back(getLlvmType(v->varType));

	llvm::FunctionType* ft = llvm::FunctionType::get(getLlvmType(this->varType), argtypes, false);
	llvm::Function* func = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, this->name, mainModule);

	// check for redef
	if(func->getName() != this->name)
		error("Redefinition of function '%s'", this->name.c_str());

	return func;
}

llvm::Value* Func::codeGen()
{
	// because the main code generator is two-pass, we expect all function declarations to have been generated
	// so just fetch it.
	symbolTable.clear();
	llvm::Function* func = mainModule->getFunction(this->decl->name);
	if(!func)
	{
		error("Failed to get function declaration for func '%s'", this->decl->name.c_str());
		return nullptr;
	}

	// unfortunately, because we have to clear the symtab above, we need to add the param vars here
	int i = 0;
	for(llvm::Function::arg_iterator it = func->arg_begin(); i != func->arg_size(); it++, i++)
	{
		it->setName(this->decl->params[i]->name);
		symbolTable[this->decl->params[i]->name] = it;
	}


	llvm::BasicBlock* block = llvm::BasicBlock::Create(llvm::getGlobalContext(), "entry", func);
	mainBuilder.SetInsertPoint(block);

	// codegen everything in the body.
	llvm::Value* lastVal = nullptr;
	for(Expr* e : this->statements)
		lastVal = e->codeGen();


	// check if we're not returning void
	if(this->decl->varType != VarType::Void)
	{
		if(this->statements.size() == 0)
		{
			error("Return value required for function '%s'", this->decl->name.c_str());
		}

		// the last expr is the final return value.
		// if we had an explicit return, then the dynamic cast will succeed and we don't need to do anything
		if(!dynamic_cast<Return*>(this->statements.back()))
		{
			// else, if the cast failed it means we didn't explicitly return, so we take the
			// value of the last expr as the return value.

			mainBuilder.CreateRet(lastVal);
		}
	}
	else
	{
		mainBuilder.CreateRetVoid();
	}

	llvm::verifyFunction(*func);
	Fpm->run(*func);
	return func;
}

llvm::Value* Root::codeGen()
{
	// two pass: first codegen all the declarations
	for(Func* f : this->functions)
		f->decl->codeGen();

	// then do the actual code
	for(Func* f : this->functions)
		f->codeGen();

	mainModule->dump();
	return nullptr;
}




namespace Codegen
{
	void doCodegen(Root* root)
	{
		llvm::FunctionPassManager OurFPM = llvm::FunctionPassManager(mainModule);

		// Set up the optimiser pipeline.  Start with registering info about how the
		// target lays out data structures.
		// OurFPM.add(new llvm::DataLayout(*llvm::TheExecutionEngine->getDataLayout()));

		// Provide basic AliasAnalysis support for GVN.
		OurFPM.add(llvm::createBasicAliasAnalysisPass());

		// Do simple "peephole" optimizations and bit-twiddling optzns.
		OurFPM.add(llvm::createInstructionCombiningPass());

		// Reassociate expressions.
		OurFPM.add(llvm::createReassociatePass());

		// Eliminate Common SubExpressions.
		OurFPM.add(llvm::createGVNPass());

		// Simplify the control flow graph (deleting unreachable blocks, etc).
		OurFPM.add(llvm::createCFGSimplificationPass());

		OurFPM.doInitialization();

		// Set the global so the code gen can use this.
		Fpm = &OurFPM;
		root->codeGen();
	}
}











