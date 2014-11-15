// LlvmCodeGen.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include <map>
#include <vector>
#include <memory>
#include <utility>
#include <cfloat>
#include <stdint.h>
#include "../include/ast.h"

#include "llvm/Analysis/Passes.h"
#include "llvm/ExecutionEngine/JIT.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/PassManager.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Transforms/Scalar.h"

using namespace Ast;
using namespace Codegen;

#define DEBUG 1
#define RUN 1

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

namespace Codegen
{
	static llvm::FunctionPassManager* Fpm;
	static llvm::ExecutionEngine* execEngine;
	static std::map<std::string, FuncDecl*> funcTable;
	static std::map<std::string, std::pair<llvm::AllocaInst*, VarDecl*>> symbolTable;
	static llvm::IRBuilder<> mainBuilder = llvm::IRBuilder<>(llvm::getGlobalContext());
	static llvm::Module* mainModule;

	void doCodegen(Root* root)
	{
		llvm::InitializeNativeTarget();
		mainModule = new llvm::Module("mainModule", llvm::getGlobalContext());

		std::string err;
		execEngine = llvm::EngineBuilder(mainModule).setErrorStr(&err).create();

		if(!execEngine)
		{
			fprintf(stderr, "%s", err.c_str());
			exit(1);
		}
		llvm::FunctionPassManager OurFPM = llvm::FunctionPassManager(mainModule);

		assert(execEngine);
		mainModule->setDataLayout(execEngine->getDataLayout());

		if(!DEBUG)
		{
			// Provide basic AliasAnalysis support for GVN.
			OurFPM.add(llvm::createBasicAliasAnalysisPass());

			// Do simple "peephole" optimisations and bit-twiddling optzns.
			OurFPM.add(llvm::createInstructionCombiningPass());

			// Reassociate expressions.
			OurFPM.add(llvm::createReassociatePass());

			// Eliminate Common SubExpressions.
			OurFPM.add(llvm::createGVNPass());

			// Simplify the control flow graph (deleting unreachable blocks, etc).
			OurFPM.add(llvm::createCFGSimplificationPass());
		}

		OurFPM.doInitialization();

		// Set the global so the code gen can use this.
		Fpm = &OurFPM;
		root->codeGen();

		mainModule->dump();





		if(RUN)
		{
			// check for a main() function and execute it
			llvm::Function* main;
			if((main = mainModule->getFunction("main")))
			{
				auto func = execEngine->getPointerToFunction(main);

				void (*ptr)() = (void(*)()) func;
				ptr();
			}
		}
	}


	static bool isBuiltinType(Expr* e)
	{
		return e->varType <= VarType::Bool || e->varType == VarType::Float32 || e->varType == VarType::Float64 || e->varType == VarType::Void;
	}


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

	static VarType determineVarType(Expr* e)
	{
		VarRef* ref;
		VarDecl* decl;
		BinOp* bo;
		Number* num;
		if((ref = dynamic_cast<VarRef*>(e)))
		{
			VarDecl* decl = symbolTable[ref->name].second;

			// it's a decl. get the type, motherfucker.
			return e->varType = Parser::determineVarType(decl->type);
		}
		else if((decl = dynamic_cast<VarDecl*>(e)))
		{
			// it's a decl. get the type, motherfucker.
			return e->varType = Parser::determineVarType(decl->type);
		}
		else if((num = dynamic_cast<Number*>(e)))
		{
			// it's a decl. get the type, motherfucker.
			return num->varType;
		}
		else if(dynamic_cast<UnaryOp*>(e))
		{
			return determineVarType(dynamic_cast<UnaryOp*>(e)->expr);
		}
		else if((bo = dynamic_cast<BinOp*>(e)))
		{
			// check what kind of shit it is
			if(bo->op == ArithmeticOp::CmpLT || bo->op == ArithmeticOp::CmpGT || bo->op == ArithmeticOp::CmpLEq
				|| bo->op == ArithmeticOp::CmpGEq || bo->op == ArithmeticOp::CmpEq || bo->op == ArithmeticOp::CmpNEq)
			{
				return VarType::Bool;
			}
			else
			{
				error("fuck off");
				return VarType::Bool;
			}
		}
		else
		{
			error("Unable to determine variable type");
			return e->varType;
		}
	}
	static bool isIntegerType(Expr* e)		{ return determineVarType(e) <= VarType::Uint64; }
	static bool isSignedType(Expr* e)		{ return determineVarType(e) <= VarType::Int64; }

	static llvm::AllocaInst* getAllocedInstanceInBlock(llvm::Function* func, VarDecl* var)
	{
		llvm::IRBuilder<> tmpBuilder(&func->getEntryBlock(), func->getEntryBlock().begin());
		return tmpBuilder.CreateAlloca(getLlvmType(var->varType), 0, var->name);
	}


	static llvm::Value* getDefaultValue(Expr* e)
	{
		VarType tp = determineVarType(e);
		switch(tp)
		{
			case VarType::Int8:		return llvm::ConstantInt::get(llvm::getGlobalContext(), llvm::APInt(8, 0, false));
			case VarType::Int16:	return llvm::ConstantInt::get(llvm::getGlobalContext(), llvm::APInt(16, 0, false));
			case VarType::Int32:	return llvm::ConstantInt::get(llvm::getGlobalContext(), llvm::APInt(32, 0, false));
			case VarType::Int64:	return llvm::ConstantInt::get(llvm::getGlobalContext(), llvm::APInt(64, 0, false));

			case VarType::Uint32:	return llvm::ConstantInt::get(llvm::getGlobalContext(), llvm::APInt(8, 0, true));
			case VarType::Uint64:	return llvm::ConstantInt::get(llvm::getGlobalContext(), llvm::APInt(16, 0, true));
			case VarType::Uint8:	return llvm::ConstantInt::get(llvm::getGlobalContext(), llvm::APInt(32, 0, true));
			case VarType::Uint16:	return llvm::ConstantInt::get(llvm::getGlobalContext(), llvm::APInt(64, 0, true));

			case VarType::Float32:	return llvm::ConstantFP::get(llvm::getGlobalContext(), llvm::APFloat(0.0f));
			case VarType::Float64:	return llvm::ConstantFP::get(llvm::getGlobalContext(), llvm::APFloat(0.0));
			case VarType::Bool:		return llvm::ConstantInt::get(llvm::getGlobalContext(), llvm::APInt(1, 0, true));

			default:				return llvm::Constant::getNullValue(llvm::Type::getVoidTy(llvm::getGlobalContext()));
		}
	}

	static std::string getReadableType(llvm::Type* type)
	{
		std::string thing;
		llvm::raw_string_ostream rso(thing);

		type->print(rso);

		return rso.str();
	}

	static Expr* autoCastNumber(Expr* left, Expr* right)
	{
		// adjust the right hand int literal, if it is one
		Number* n = nullptr;
		if((n = dynamic_cast<Number*>(right)) || (dynamic_cast<UnaryOp*>(right) && (n = dynamic_cast<Number*>(dynamic_cast<UnaryOp*>(right)->expr))))
		{
			if(determineVarType(left) == VarType::Int8 && n->ival <= INT8_MAX)			right->varType = VarType::Int8; //, printf("i8");
			else if(determineVarType(left) == VarType::Int16 && n->ival <= INT16_MAX)	right->varType = VarType::Int16; //, printf("i16");
			else if(determineVarType(left) == VarType::Int32 && n->ival <= INT32_MAX)	right->varType = VarType::Int32; //, printf("i32");
			else if(determineVarType(left) == VarType::Int64 && n->ival <= INT64_MAX)	right->varType = VarType::Int64; //, printf("i64");
			else if(determineVarType(left) == VarType::Uint8 && n->ival <= UINT8_MAX)	right->varType = VarType::Uint8; //, printf("u8");
			else if(determineVarType(left) == VarType::Uint16 && n->ival <= UINT16_MAX)	right->varType = VarType::Uint16; //, printf("u16");
			else if(determineVarType(left) == VarType::Uint32 && n->ival <= UINT32_MAX)	right->varType = VarType::Uint32; //, printf("u32");
			else if(determineVarType(left) == VarType::Uint64 && n->ival <= UINT64_MAX)	right->varType = VarType::Uint64; //, printf("u64");
			else if(determineVarType(left) == VarType::Float32 && n->dval <= FLT_MAX)	right->varType = VarType::Float32; //, printf("f32");
			else if(determineVarType(left) == VarType::Float64 && n->dval <= DBL_MAX)	right->varType = VarType::Float64; //, printf("f64");
			else
			{
				error("Cannot assign to target, it is too small.");
			}

			assert(determineVarType(left) == determineVarType(right));
			return right;
		}

		error("Could not convert number");
		return nullptr;
	}
}



llvm::Value* Number::codeGen()
{
	// check builtin type
	if(this->varType <= VarType::Uint64)
		return llvm::ConstantInt::get(llvm::getGlobalContext(), llvm::APInt(pow(2, (int) this->varType % 4) * 8, this->ival, this->varType > VarType::Int64));

	else if(this->type == "Float32" || this->type == "Float64")
		return llvm::ConstantFP::get(llvm::getGlobalContext(), llvm::APFloat(this->dval));


	error("WHAT");
	return nullptr;
}

llvm::Value* VarRef::codeGen()
{
	llvm::Value* val = Codegen::symbolTable[this->name].first;

	if(!val)
		error("Unknown variable name '%s'", this->name.c_str());

	return mainBuilder.CreateLoad(val, this->name);
}

llvm::Value* VarDecl::codeGen()
{
	llvm::Function* func = mainBuilder.GetInsertBlock()->getParent();
	llvm::Value* val = nullptr;

	llvm::AllocaInst* ai = getAllocedInstanceInBlock(func, this);
	symbolTable[this->name] = std::pair<llvm::AllocaInst*, VarDecl*>(ai, this);

	if(this->initVal)
	{
		this->initVal = autoCastNumber(this, this->initVal);
		val = this->initVal->codeGen();
	}
	else
	{
		val = getDefaultValue(this);
	}

	mainBuilder.CreateStore(val, ai);
	return val;
}

llvm::Value* Return::codeGen()
{
	return mainBuilder.CreateRet(this->val->codeGen());
}

llvm::Value* FuncCall::codeGen()
{
	llvm::Function* target = mainModule->getFunction(this->name);
	if(target == 0)
		error("Unknown function '%s'", this->name.c_str());

	if(target->arg_size() != this->params.size())
		error("Expected %ld arguments, but got %ld arguments instead", target->arg_size(), this->params.size());

	std::vector<llvm::Value*> args;
	llvm::Function::arg_iterator it = target->arg_begin();

	// we need to get the function declaration
	FuncDecl* decl = funcTable[this->name];
	assert(decl);

	for(int i = 0; i < this->params.size(); i++)
		this->params[i] = autoCastNumber(decl->params[i], this->params[i]);

	for(Expr* e : this->params)
	{
		args.push_back(e->codeGen());
		if(args.back() == nullptr)
			return 0;

		it++;
	}

	return mainBuilder.CreateCall(target, args);
}

llvm::Value* FuncDecl::codeGen()
{
	std::vector<llvm::Type*> argtypes;
	for(VarDecl* v : this->params)
		argtypes.push_back(getLlvmType(v->varType));

	llvm::FunctionType* ft = llvm::FunctionType::get(getLlvmType(this->varType), argtypes, false);
	llvm::Function* func = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, this->name, mainModule);

	// check for redef
	if(func->getName() != this->name)
		error("Redefinition of function '%s'", this->name.c_str());

	funcTable[this->name] = this;
	return func;
}

llvm::Value* ForeignFuncDecl::codeGen()
{
	return this->decl->codeGen();
}

llvm::Value* Closure::codeGen()
{
	llvm::Value* lastVal = nullptr;
	for(Expr* e : this->statements)
		lastVal = e->codeGen();

	return lastVal;
}

void codeGenRecursiveIf(llvm::Function* func, std::deque<std::pair<Expr*, Closure*>> pairs, llvm::BasicBlock* merge, llvm::PHINode* phi)
{
	if(pairs.size() == 0)
		return;

	llvm::BasicBlock* t = llvm::BasicBlock::Create(llvm::getGlobalContext(), "trueCaseR", func);
	llvm::BasicBlock* f = llvm::BasicBlock::Create(llvm::getGlobalContext(), "falseCaseR");

	llvm::Value* cond = pairs.front().first->codeGen();

	VarType apprType = determineVarType(pairs.front().first);
	if(apprType != VarType::Bool)
		cond = mainBuilder.CreateICmpNE(cond, llvm::ConstantInt::get(llvm::getGlobalContext(), llvm::APInt(pow(2, (int) apprType % 4) * 8, 0, apprType > VarType::Int64)), "ifCond");

	else
		cond = mainBuilder.CreateICmpNE(cond, llvm::ConstantInt::get(llvm::getGlobalContext(), llvm::APInt(1, false, true)));



	mainBuilder.CreateCondBr(cond, t, f);
	mainBuilder.SetInsertPoint(t);

	llvm::Value* val = pairs.front().second->codeGen();

	if(phi)
		phi->addIncoming(val, t);

	mainBuilder.CreateBr(merge);


	// now the false case...
	// set the insert point to the false case, then go again.
	mainBuilder.SetInsertPoint(f);

	// recursively call ourselves
	pairs.pop_front();
	codeGenRecursiveIf(func, pairs, merge, phi);

	// once that's done, we can add the false-case block to the func
	func->getBasicBlockList().push_back(f);
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
			assert(0);
	}
}


llvm::Value* If::codeGen()
{
	assert(this->cases.size() > 0);
	llvm::Value* firstCond = this->cases[0].first->codeGen();
	VarType apprType = determineVarType(this->cases[0].first);

	if(apprType != VarType::Bool)
		firstCond = mainBuilder.CreateICmpNE(firstCond, llvm::ConstantInt::get(llvm::getGlobalContext(), llvm::APInt(pow(2, (int) apprType % 4) * 8, 0, apprType > VarType::Int64)), "ifCond");

	else
		firstCond = mainBuilder.CreateICmpNE(firstCond, llvm::ConstantInt::get(llvm::getGlobalContext(), llvm::APInt(1, false, true)));


	llvm::Function* func = mainBuilder.GetInsertBlock()->getParent();
	llvm::BasicBlock* trueb = llvm::BasicBlock::Create(llvm::getGlobalContext(), "trueCase", func);
	llvm::BasicBlock* falseb = llvm::BasicBlock::Create(llvm::getGlobalContext(), "falseCase");
	llvm::BasicBlock* merge = llvm::BasicBlock::Create(llvm::getGlobalContext(), "merge");


	// create the first conditional
	mainBuilder.CreateCondBr(firstCond, trueb, falseb);


	// emit code for the first block
	mainBuilder.SetInsertPoint(trueb);
	llvm::Value* truev = this->cases[0].second->codeGen();
	// assert(truev);
	mainBuilder.CreateBr(merge);

	// now for the clusterfuck.
	// to support if-elseif-elseif-elseif-...-else, we need to essentially compound/cascade conditionals in the 'else' block
	// of the if statement.

	mainBuilder.SetInsertPoint(falseb);

	auto c1 = this->cases.front();
	this->cases.pop_front();

	llvm::BasicBlock* curblk = mainBuilder.GetInsertBlock();
	mainBuilder.SetInsertPoint(merge);

	// llvm::PHINode* phi = mainBuilder.CreatePHI(llvm::Type::getVoidTy(llvm::getGlobalContext()), this->cases.size() + (this->final ? 1 : 0));

	llvm::PHINode* phi = nullptr;

	if(phi)
		phi->addIncoming(truev, trueb);

	mainBuilder.SetInsertPoint(curblk);
	codeGenRecursiveIf(func, std::deque<std::pair<Expr*, Closure*>>(this->cases), merge, phi);

	func->getBasicBlockList().push_back(falseb);

	// if we have an 'else' case
	if(this->final)
	{
		llvm::Value* v = this->final->codeGen();

		if(phi)
			phi->addIncoming(v, falseb);
	}

	mainBuilder.CreateBr(merge);

	func->getBasicBlockList().push_back(merge);
	mainBuilder.SetInsertPoint(merge);

	// return false
	return llvm::ConstantInt::get(llvm::getGlobalContext(), llvm::APInt(1, 0, true));
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


	llvm::BasicBlock* block = llvm::BasicBlock::Create(llvm::getGlobalContext(), "entry", func);
	mainBuilder.SetInsertPoint(block);



	// unfortunately, because we have to clear the symtab above, we need to add the param vars here
	int i = 0;
	for(llvm::Function::arg_iterator it = func->arg_begin(); i != func->arg_size(); it++, i++)
	{
		it->setName(this->decl->params[i]->name);

		llvm::AllocaInst* ai = getAllocedInstanceInBlock(func, this->decl->params[i]);
		mainBuilder.CreateStore(it, ai);

		symbolTable[this->decl->params[i]->name] = std::pair<llvm::AllocaInst*, VarDecl*>(ai, this->decl->params[i]);
	}



	// codegen everything in the body.
	llvm::Value* lastVal = this->closure->codeGen();

	// check if we're not returning void
	if(this->decl->varType != VarType::Void)
	{
		if(this->closure->statements.size() == 0)
			error("Return value required for function '%s'", this->decl->name.c_str());

		// the last expr is the final return value.
		// if we had an explicit return, then the dynamic cast will succeed and we don't need to do anything
		if(!dynamic_cast<Return*>(this->closure->statements.back()))
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

	if(!DEBUG)
		Fpm->run(*func);

	return func;
}

llvm::Value* BinOp::codeGen()
{
	llvm::Value* lhs;
	llvm::Value* rhs;


	this->right = autoCastNumber(this->left, this->right);
	lhs = this->left->codeGen();
	rhs = this->right->codeGen();

	if(this->op == ArithmeticOp::Assign)
	{
		VarRef* v;
		if(!(v = dynamic_cast<VarRef*>(this->left)))
			error("Left-hand side of assignment must be assignable");

		if(!rhs)
			error("What?");

		llvm::Value* var = symbolTable[v->name].first;
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

llvm::Value* Root::codeGen()
{
	// two pass: first codegen all the declarations
	for(ForeignFuncDecl* f : this->foreignfuncs)
		f->codeGen();

	for(Func* f : this->functions)
		f->decl->codeGen();

	// then do the actual code
	for(Func* f : this->functions)
		f->codeGen();

	return nullptr;
}








#if DEBUG

extern "C" void printInt32(uint32_t i)
{
	printf("%d", i);
}

extern "C" void printInt64(uint64_t i)
{
	printf("%lld", i);
}

#endif











