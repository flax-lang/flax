// LlvmCodeGen.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include <map>
#include <vector>
#include <memory>
#include <utility>
#include <cfloat>
#include <stdint.h>
#include <typeinfo>
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

enum class ExprType
{
	Struct,
	Func
};

namespace Codegen
{
	typedef std::pair<llvm::AllocaInst*, VarDecl*> SymbolPair_t;
	typedef std::map<std::string, SymbolPair_t> SymTab_t;
	typedef std::pair<Expr*, ExprType> TypedExpr_t;
	typedef std::pair<llvm::Type*, TypedExpr_t> TypePair_t;
	typedef std::map<std::string, TypePair_t> TypeMap_t;


	static llvm::Module* mainModule;
	static llvm::FunctionPassManager* Fpm;
	static std::deque<SymTab_t*> symTabStack;
	static llvm::ExecutionEngine* execEngine;
	static std::deque<TypeMap_t*> visibleTypes;
	static std::map<std::string, FuncDecl*> funcTable;
	static llvm::IRBuilder<> mainBuilder = llvm::IRBuilder<>(llvm::getGlobalContext());

	static void popScope();
	static void pushScope();
	static void pushScope(SymTab_t* tab, TypePair_t* tp);



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

		pushScope();
		root->codeGen();
		popScope();

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

			printf("\n\n");
		}
	}

	static bool isSignedType(Expr* e);
	static bool isBuiltinType(Expr* e);
	static bool isIntegerType(Expr* e);
	static VarType determineVarType(Expr* e);
	static llvm::Type* getLlvmType(Expr* expr);
	static llvm::Value* getDefaultValue(Expr* e);
	static std::string getReadableType(Expr* expr);
	static Expr* autoCastType(Expr* left, Expr* right);
	static std::string getReadableType(llvm::Type* type);
	static llvm::AllocaInst* getAllocedInstanceInBlock(llvm::Function* func, VarDecl* var);























	static llvm::LLVMContext& getContext()
	{
		return mainModule->getContext();
	}

	static void popScope()
	{
		SymTab_t* tab = symTabStack.back();
		TypeMap_t* types = visibleTypes.back();

		delete types;
		delete tab;

		symTabStack.pop_back();
		visibleTypes.pop_back();
	}

	static void pushScope(SymTab_t* tab, TypeMap_t* tp)
	{
		symTabStack.push_back(tab);
		visibleTypes.push_back(tp);
	}

	static void pushScope()
	{
		pushScope(new SymTab_t(), new TypeMap_t());
	}

	static SymTab_t& getSymTab()
	{
		return *symTabStack.back();
	}

	static SymbolPair_t* getSymPair(const std::string& name)
	{
		// loop.
		for(int i = symTabStack.size(); i-- > 0;)
		{
			SymTab_t* tab = symTabStack[i];

			if(tab->find(name) != tab->end())
				return &(*tab)[name];
		}

		return nullptr;
	}

	static llvm::Value* getSymInst(const std::string& name)
	{
		SymbolPair_t* pair = nullptr;
		if((pair = getSymPair(name)))
			return pair->first;

		return nullptr;
	}

	static VarDecl* getSymDecl(const std::string& name)
	{
		SymbolPair_t* pair = nullptr;
		if((pair = getSymPair(name)))
			return pair->second;

		return nullptr;
	}

	static bool isDuplicateSymbol(const std::string& name)
	{
		return getSymTab().find(name) != getSymTab().end();
	}



	static TypeMap_t& getVisibleTypes()
	{
		return *visibleTypes.back();
	}

	static TypePair_t* getType(std::string name)
	{
		for(TypeMap_t* map : visibleTypes)
		{
			if(map->find(name) != map->end())
				return &(*map)[name];
		}

		return nullptr;
	}

	static bool isDuplicateType(std::string name)
	{
		return getType(name) != nullptr;
	}




















	static bool isBuiltinType(Expr* e)
	{
		return e->varType <= VarType::Bool || e->varType == VarType::Float32 || e->varType == VarType::Float64 || e->varType == VarType::Void;
	}

	static llvm::Type* getLlvmType(Expr* expr)
	{
		VarType t;

		assert(expr);
		if((t = determineVarType(expr)) != VarType::UserDefined)
		{
			switch(t)
			{
				case VarType::Uint8:
				case VarType::Int8:		return llvm::Type::getInt8Ty(getContext());

				case VarType::Uint16:
				case VarType::Int16:	return llvm::Type::getInt16Ty(getContext());

				case VarType::Uint32:
				case VarType::Int32:	return llvm::Type::getInt32Ty(getContext());

				case VarType::Uint64:
				case VarType::Int64:	return llvm::Type::getInt64Ty(getContext());

				case VarType::Float32:	return llvm::Type::getFloatTy(getContext());
				case VarType::Float64:	return llvm::Type::getDoubleTy(getContext());

				case VarType::Void:		return llvm::Type::getVoidTy(getContext());

				default:
					error("(%s:%s:%d) -> Internal check failed: invalid type", __FILE__, __PRETTY_FUNCTION__, __LINE__);
					return nullptr;
			}
		}
		else
		{
			VarRef* ref = nullptr;
			if(dynamic_cast<VarDecl*>(expr))
			{
				TypePair_t* type = getType(expr->type);
				if(!type)
					error("Unknown type '%s'", expr->type.c_str());

				return type->first;
			}
			else if((ref = dynamic_cast<VarRef*>(expr)))
			{
				return getLlvmType(getSymDecl(ref->name));
			}
		}

		return nullptr;
	}

	static VarType determineVarType(Expr* e)
	{
		VarRef* ref = nullptr;
		VarDecl* decl = nullptr;
		BinOp* bo = nullptr;
		Number* num = nullptr;
		FuncDecl* fd = nullptr;
		if((ref = dynamic_cast<VarRef*>(e)))
		{
			VarDecl* decl = getSymTab()[ref->name].second;
			if(!decl)
				error("Unknown variable '%s'", ref->name.c_str());

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
		else if(dynamic_cast<Func*>(e))
		{
			return determineVarType(dynamic_cast<Func*>(e)->decl);
		}
		else if((fd = dynamic_cast<FuncDecl*>(e)))
		{
			return Parser::determineVarType(fd->type);
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
				// need to determine type on both sides.
				bo->left = autoCastType(bo->left, bo->right);

				// make sure that now, both sides are the same.
				if(determineVarType(bo->left) != determineVarType(bo->right))
					error("Unable to form binary expression with different types '%s' and '%s'", getReadableType(bo->left).c_str(), getReadableType(bo->right).c_str());


				return determineVarType(bo->left);
			}
		}
		else
		{
			// error("Unable to determine var type - '%s'", e->type.c_str());
			return VarType::UserDefined;
		}
	}

	static bool isIntegerType(Expr* e)		{ return determineVarType(e) <= VarType::Uint64; }
	static bool isSignedType(Expr* e)		{ return determineVarType(e) <= VarType::Int64; }

	static llvm::AllocaInst* getAllocedInstanceInBlock(llvm::Function* func, VarDecl* var)
	{
		llvm::IRBuilder<> tmpBuilder(&func->getEntryBlock(), func->getEntryBlock().begin());
		return tmpBuilder.CreateAlloca(getLlvmType(var), 0, var->name);
	}


	static llvm::Value* getDefaultValue(Expr* e)
	{
		VarType tp = determineVarType(e);
		switch(tp)
		{
			case VarType::Int8:		return llvm::ConstantInt::get(getContext(), llvm::APInt(8, 0, false));
			case VarType::Int16:	return llvm::ConstantInt::get(getContext(), llvm::APInt(16, 0, false));
			case VarType::Int32:	return llvm::ConstantInt::get(getContext(), llvm::APInt(32, 0, false));
			case VarType::Int64:	return llvm::ConstantInt::get(getContext(), llvm::APInt(64, 0, false));

			case VarType::Uint32:	return llvm::ConstantInt::get(getContext(), llvm::APInt(8, 0, true));
			case VarType::Uint64:	return llvm::ConstantInt::get(getContext(), llvm::APInt(16, 0, true));
			case VarType::Uint8:	return llvm::ConstantInt::get(getContext(), llvm::APInt(32, 0, true));
			case VarType::Uint16:	return llvm::ConstantInt::get(getContext(), llvm::APInt(64, 0, true));

			case VarType::Float32:	return llvm::ConstantFP::get(getContext(), llvm::APFloat(0.0f));
			case VarType::Float64:	return llvm::ConstantFP::get(getContext(), llvm::APFloat(0.0));
			case VarType::Bool:		return llvm::ConstantInt::get(getContext(), llvm::APInt(1, 0, true));

			default:				return llvm::Constant::getNullValue(getLlvmType(e));
		}
	}

	static std::string getReadableType(llvm::Type* type)
	{
		std::string thing;
		llvm::raw_string_ostream rso(thing);

		type->print(rso);

		return rso.str();
	}

	static std::string getReadableType(Expr* expr)
	{
		return getReadableType(getLlvmType(expr));
	}

	static Expr* autoCastType(Expr* left, Expr* right)
	{
		// adjust the right hand int literal, if it is one
		Number* n = nullptr;
		BinOp* b = nullptr;
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

		// ignore it if we can't convert it, likely it is a more complex expression or a varRef.
		return right;
	}
}

















// begin codegen impl
// todo: split up or organise

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

	llvm::AllocaInst* ai = getAllocedInstanceInBlock(func, this);
	getSymTab()[this->name] = std::pair<llvm::AllocaInst*, VarDecl*>(ai, this);

	if(this->initVal)
	{
		this->initVal = autoCastType(this, this->initVal);
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
		this->params[i] = autoCastType(decl->params[i], this->params[i]);

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
	std::string mangledname;

	std::vector<llvm::Type*> argtypes;
	for(VarDecl* v : this->params)
	{
		mangledname += "_" + getReadableType(v);
		argtypes.push_back(getLlvmType(v));
	}

	// check if empty and if it's an extern. mangle the name to include type info if possible.
	if(!mangledname.empty() && !this->isFFI)
		this->name += "@" + mangledname;

	llvm::FunctionType* ft = llvm::FunctionType::get(getLlvmType(this), argtypes, false);
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



void codeGenRecursiveIf(llvm::Function* func, std::deque<std::pair<Expr*, Closure*>> pairs, llvm::BasicBlock* merge, llvm::PHINode* phi)
{
	if(pairs.size() == 0)
		return;

	llvm::BasicBlock* t = llvm::BasicBlock::Create(getContext(), "trueCaseR", func);
	llvm::BasicBlock* f = llvm::BasicBlock::Create(getContext(), "falseCaseR");

	llvm::Value* cond = pairs.front().first->codeGen();


	VarType apprType = determineVarType(pairs.front().first);
	if(apprType != VarType::Bool)
		cond = mainBuilder.CreateICmpNE(cond, llvm::ConstantInt::get(getContext(), llvm::APInt(pow(2, (int) apprType % 4) * 8, 0, apprType > VarType::Int64)), "ifCond");

	else
		cond = mainBuilder.CreateICmpNE(cond, llvm::ConstantInt::get(getContext(), llvm::APInt(1, false, true)));



	mainBuilder.CreateCondBr(cond, t, f);
	mainBuilder.SetInsertPoint(t);

	llvm::Value* val = nullptr;
	{
		pushScope();
		val = pairs.front().second->codeGen();
		popScope();
	}

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

llvm::Value* If::codeGen()
{
	assert(this->cases.size() > 0);
	llvm::Value* firstCond = this->cases[0].first->codeGen();
	VarType apprType = determineVarType(this->cases[0].first);

	if(apprType != VarType::Bool)
		firstCond = mainBuilder.CreateICmpNE(firstCond, llvm::ConstantInt::get(getContext(), llvm::APInt(pow(2, (int) apprType % 4) * 8, 0, apprType > VarType::Int64)), "ifCond");

	else
		firstCond = mainBuilder.CreateICmpNE(firstCond, llvm::ConstantInt::get(getContext(), llvm::APInt(1, false, true)));


	llvm::Function* func = mainBuilder.GetInsertBlock()->getParent();
	llvm::BasicBlock* trueb = llvm::BasicBlock::Create(getContext(), "trueCase", func);
	llvm::BasicBlock* falseb = llvm::BasicBlock::Create(getContext(), "falseCase");
	llvm::BasicBlock* merge = llvm::BasicBlock::Create(getContext(), "merge");

	// create the first conditional
	mainBuilder.CreateCondBr(firstCond, trueb, falseb);



	// emit code for the first block
	llvm::Value* truev = nullptr;
	{
		mainBuilder.SetInsertPoint(trueb);

		// push a new symtab
		pushScope();
		truev = this->cases[0].second->codeGen();
		popScope();

		mainBuilder.CreateBr(merge);
	}



	// now for the clusterfuck.
	// to support if-elseif-elseif-elseif-...-else, we need to essentially compound/cascade conditionals in the 'else' block
	// of the if statement.

	mainBuilder.SetInsertPoint(falseb);

	auto c1 = this->cases.front();
	this->cases.pop_front();

	llvm::BasicBlock* curblk = mainBuilder.GetInsertBlock();
	mainBuilder.SetInsertPoint(merge);

	// llvm::PHINode* phi = mainBuilder.CreatePHI(llvm::Type::getVoidTy(getContext()), this->cases.size() + (this->final ? 1 : 0));

	llvm::PHINode* phi = nullptr;

	if(phi)
		phi->addIncoming(truev, trueb);

	mainBuilder.SetInsertPoint(curblk);
	codeGenRecursiveIf(func, std::deque<std::pair<Expr*, Closure*>>(this->cases), merge, phi);

	func->getBasicBlockList().push_back(falseb);

	// if we have an 'else' case
	if(this->final)
	{
		pushScope();
		llvm::Value* v = this->final->codeGen();
		popScope();

		if(phi)
			phi->addIncoming(v, falseb);
	}

	mainBuilder.CreateBr(merge);

	func->getBasicBlockList().push_back(merge);
	mainBuilder.SetInsertPoint(merge);

	// return false
	return llvm::ConstantInt::get(getContext(), llvm::APInt(1, 0, true));
}





llvm::Value* Func::codeGen()
{
	// because the main code generator is two-pass, we expect all function declarations to have been generated
	// so just fetch it.

	llvm::Function* func = mainModule->getFunction(this->decl->name);
	if(!func)
	{
		error("(%s:%s:%d) -> Internal check failed: Failed to get function declaration for func '%s'", __FILE__, __PRETTY_FUNCTION__, __LINE__, this->decl->name.c_str());
		return nullptr;
	}

	// we need to clear all previous blocks' symbols
	// but we can't destroy them, so employ a stack method.
	// create a new 'table' for our own usage
	pushScope();

	llvm::BasicBlock* block = llvm::BasicBlock::Create(getContext(), "entry", func);
	mainBuilder.SetInsertPoint(block);


	// unfortunately, because we have to clear the symtab above, we need to add the param vars here
	int i = 0;
	for(llvm::Function::arg_iterator it = func->arg_begin(); i != func->arg_size(); it++, i++)
	{
		it->setName(this->decl->params[i]->name);

		llvm::AllocaInst* ai = getAllocedInstanceInBlock(func, this->decl->params[i]);
		mainBuilder.CreateStore(it, ai);

		getSymTab()[this->decl->params[i]->name] = std::pair<llvm::AllocaInst*, VarDecl*>(ai, this->decl->params[i]);
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


	// we've codegen'ed that stuff, pop the symbol table
	popScope();

	return func;
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


llvm::Value* Struct::codeGen()
{
	std::vector<llvm::Type*> types;
	if(isDuplicateType(this->name))
		error("Duplicate type '%s'", this->name.c_str());

	// check if there's an explicit initialiser
	bool hasInit = false;
	for(Func* func : this->funcs)
	{
		if(func->decl->name == "init")
			hasInit = true;

		std::vector<llvm::Type*> args;
		for(VarDecl* v : func->decl->params)
			args.push_back(getLlvmType(v));

		types.push_back(llvm::PointerType::get(llvm::FunctionType::get(getLlvmType(func), llvm::ArrayRef<llvm::Type*>(args), false), 0));
	}


	// create llvm types
	for(VarDecl* var : this->members)
		types.push_back(getLlvmType(var));


	llvm::StructType* str = llvm::StructType::create(getContext(), llvm::ArrayRef<llvm::Type*>(types), this->name);
	getVisibleTypes()[this->name] = TypePair_t(str, TypedExpr_t(this, ExprType::Struct));


	if(!hasInit)
	{
		// create one
		llvm::FunctionType* ft = llvm::FunctionType::get(str, false);
		llvm::Function* func = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "__automatic_init@" + this->name, mainModule);

		llvm::BasicBlock* block = llvm::BasicBlock::Create(getContext(), "initialiser", func);

		llvm::BasicBlock* old = mainBuilder.GetInsertBlock();
		mainBuilder.SetInsertPoint(block);

		for(VarDecl* var : this->members)
			var->codeGen();

		// mainBuilder.SetInsertPoint(old);
	}










	return 0;
}

llvm::Value* MemberAccess::codeGen()
{
	// gen the var ref on the left.
	this->target->codeGen();
	llvm::Type* type = getLlvmType(this->target);
	if(!type)
		error("(%s:%s:%d) -> Internal check failed: invalid type encountered", __FILE__, __PRETTY_FUNCTION__, __LINE__);

	if(!type->isStructTy())
		error("Cannot do member access on non-aggregate types");

	for(int i = 0; i < type->getStructNumElements(); i++)
		printf("[%s]\n", getReadableType(type->getStructElementType(i)).c_str());


//      llvm::FunctionType* ty = llvm::FunctionType::get(llvm::Type::getInt32Ty(TargetModule->getContext()),structRegPtr,false);
//      llvm::Constant *c = TargetModule->getOrInsertFunction(BB.getBlockName(),ty);
//      fTest = llvm::cast<llvm::Function>(c);
//      fTest ->setCallingConv(llvm::CallingConv::C);
//
// //Access the struct members
// llvm:Value *ArgValuePtr = builder->CreateStructGEP(fTest ->arg_begin(),0,"ptrMember1");
// llvm::Valuie *StructValue = builder->CreateLoad(ArgValuePtr,false,"member1");


	return 0;
}









llvm::Value* Root::codeGen()
{
	// two pass: first codegen all the declarations
	for(ForeignFuncDecl* f : this->foreignfuncs)
		f->codeGen();

	for(Func* f : this->functions)
		f->decl->codeGen();

	for(Struct* s : this->structs)
		s->codeGen();



	// then do the actual code
	for(Func* f : this->functions)
		f->codeGen();

	return nullptr;
}





#if RUN

extern "C" void printInt32(uint32_t i)
{
	printf("%d", i);
}

extern "C" void printInt64(uint64_t i)
{
	printf("%lld", i);
}

#endif











