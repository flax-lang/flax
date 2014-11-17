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
#include "../include/codegen.h"
#include "../include/llvm_all.h"

using namespace Ast;
using namespace Codegen;

#define RUN 1

void error(const char* msg, ...)
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
	llvm::Module* mainModule;
	llvm::FunctionPassManager* Fpm;
	std::deque<SymTab_t*> symTabStack;
	llvm::ExecutionEngine* execEngine;
	std::deque<TypeMap_t*> visibleTypes;
	std::map<std::string, FuncDecl*> funcTable;
	llvm::IRBuilder<> mainBuilder = llvm::IRBuilder<>(llvm::getGlobalContext());

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






















	llvm::LLVMContext& getContext()
	{
		return mainModule->getContext();
	}

	void popScope()
	{
		SymTab_t* tab = symTabStack.back();
		TypeMap_t* types = visibleTypes.back();

		delete types;
		delete tab;

		symTabStack.pop_back();
		visibleTypes.pop_back();
	}

	void pushScope(SymTab_t* tab, TypeMap_t* tp)
	{
		symTabStack.push_back(tab);
		visibleTypes.push_back(tp);
	}

	void pushScope()
	{
		pushScope(new SymTab_t(), new TypeMap_t());
	}

	SymTab_t& getSymTab()
	{
		return *symTabStack.back();
	}

	SymbolPair_t* getSymPair(const std::string& name)
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

	llvm::Value* getSymInst(const std::string& name)
	{
		SymbolPair_t* pair = nullptr;
		if((pair = getSymPair(name)))
			return pair->first;

		return nullptr;
	}

	VarDecl* getSymDecl(const std::string& name)
	{
		SymbolPair_t* pair = nullptr;
		if((pair = getSymPair(name)))
			return pair->second;

		return nullptr;
	}

	bool isDuplicateSymbol(const std::string& name)
	{
		return getSymTab().find(name) != getSymTab().end();
	}



	TypeMap_t& getVisibleTypes()
	{
		return *visibleTypes.back();
	}

	TypePair_t* getType(std::string name)
	{
		for(TypeMap_t* map : visibleTypes)
		{
			if(map->find(name) != map->end())
				return &(*map)[name];
		}

		return nullptr;
	}

	bool isDuplicateType(std::string name)
	{
		return getType(name) != nullptr;
	}

	bool isBuiltinType(Expr* expr)
	{
		VarType e = determineVarType(expr);
		return e <= VarType::Bool || e == VarType::Float32 || e == VarType::Float64 || e == VarType::Void;
	}

	bool isPtr(Expr* expr)
	{
		VarType e = determineVarType(expr);
		return (e >= VarType::Int8Ptr && e <= VarType::Uint64Ptr) || e == VarType::AnyPtr;
	}

	llvm::Type* getLlvmTypeOfBuiltin(VarType t)
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

			case VarType::Uint8Ptr:
			case VarType::Int8Ptr:	return llvm::Type::getInt8PtrTy(getContext());

			case VarType::Uint16Ptr:
			case VarType::Int16Ptr:	return llvm::Type::getInt16PtrTy(getContext());

			case VarType::Uint32Ptr:
			case VarType::Int32Ptr:	return llvm::Type::getInt32PtrTy(getContext());

			case VarType::Uint64Ptr:
			case VarType::Int64Ptr:	return llvm::Type::getInt64PtrTy(getContext());


			case VarType::Void:		return llvm::Type::getVoidTy(getContext());
			case VarType::Bool:		return llvm::Type::getInt1Ty(getContext());

			default:
				error("(%s:%s:%d) -> Internal check failed: not a builtin type", __FILE__, __PRETTY_FUNCTION__, __LINE__);
				return nullptr;
		}
	}

	llvm::Type* getLlvmType(Expr* expr)
	{
		VarType t;

		assert(expr);
		if((t = determineVarType(expr)) != VarType::UserDefined && t != VarType::Array)
		{
			return getLlvmTypeOfBuiltin(t);
		}
		else
		{
			VarRef* ref = nullptr;
			VarDecl* decl = nullptr;
			if((decl = dynamic_cast<VarDecl*>(expr)))
			{
				if(t != VarType::Array)
				{
					TypePair_t* type = getType(expr->type);
					if(!type)
						error("Unknown type '%s'", expr->type.c_str());

					return type->first;
				}





				// it's an array. decide on its size.
				size_t pos = decl->type.find_first_of('[');
				if(pos == std::string::npos)
					error("(%s:%s:%d) -> Internal check failed: invalid array declaration string", __FILE__, __PRETTY_FUNCTION__, __LINE__);

				std::string etype = decl->type.substr(0, pos);
				std::string atype = decl->type.substr(pos);
				assert(atype[0] == '[' && atype.back() == ']');

				std::string num = atype.substr(1).substr(0, atype.length() - 2);
				int sz = std::stoi(num);
				if(sz == 0)
					error("Dynamically sized arrays are not yet supported");

				VarType evt = Parser::determineVarType(etype);

				llvm::Type* eltype = nullptr;
				if(evt == VarType::Array)
					error("Nested arrays are not yet supported");

				if(evt == VarType::Void)
					error("You cannot create an array of void");

				if(evt != VarType::UserDefined)
				{
					eltype = getLlvmTypeOfBuiltin(evt);
				}
				else
				{
					TypePair_t* type = getType(etype);
					if(!type)
						error("Unknown type '%s'", etype.c_str());

					eltype = type->first;
				}

				return llvm::ArrayType::get(eltype, sz);
			}
			else if((ref = dynamic_cast<VarRef*>(expr)))
			{
				return getLlvmType(getSymDecl(ref->name));
			}
		}

		return nullptr;
	}

	VarType determineVarType(Expr* e)
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

	bool isIntegerType(Expr* e)		{ return determineVarType(e) <= VarType::Uint64; }
	bool isSignedType(Expr* e)		{ return determineVarType(e) <= VarType::Int64; }

	llvm::AllocaInst* allocateInstanceInBlock(llvm::Function* func, llvm::Type* type, std::string name)
	{
		llvm::IRBuilder<> tmpBuilder(&func->getEntryBlock(), func->getEntryBlock().begin());
		return tmpBuilder.CreateAlloca(type, 0, name);
	}

	llvm::AllocaInst* allocateInstanceInBlock(llvm::Function* func, VarDecl* var)
	{
		return allocateInstanceInBlock(func, getLlvmType(var), var->name);
	}


	llvm::Value* getDefaultValue(Expr* e)
	{
		llvm::Type* llvmtype = getLlvmType(e);

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

			case VarType::Array:
			{
				assert(llvmtype->isArrayTy());
				llvm::ArrayType* at = llvm::cast<llvm::ArrayType>(llvmtype);

				std::vector<llvm::Constant*> els;
				for(uint64_t i = 0; i < at->getNumElements(); i++)
					els.push_back(llvm::ConstantArray::getNullValue(at->getElementType()));

				return llvm::ConstantArray::get(at, els);
			}

			// todo: check for pointer type
			default:				return llvm::Constant::getNullValue(getLlvmType(e));
		}
	}

	std::string getReadableType(llvm::Type* type)
	{
		std::string thing;
		llvm::raw_string_ostream rso(thing);

		type->print(rso);

		return rso.str();
	}

	std::string getReadableType(Expr* expr)
	{
		return getReadableType(getLlvmType(expr));
	}

	Expr* autoCastType(Expr* left, Expr* right)
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

			return right;
		}

		// ignore it if we can't convert it, likely it is a more complex expression or a varRef.
		return right;
	}

	std::string mangleName(Struct* s, std::string orig)
	{
		return "__struct@" + s->name + "_" + orig;
	}

	std::string unmangleName(Struct* s, std::string orig)
	{
		std::string ret = orig;
		if(orig.find("__struct@") != 0)
			error("'%s' is not a mangled name of a struct.", orig.c_str());


		if(orig.length() < 10 || orig[9] != '_')
			error("Invalid mangled name '%s'", orig.c_str());


		// remove __struct@_
		ret = ret.substr(10);

		// make sure it's the right struct.
		if(ret.find(s->name) != 0)
			error("'%s' is not a mangled name of struct '%s'", orig.c_str(), s->name.c_str());

		return ret.substr(s->name.length());
	}

	bool isArrayType(Expr* e)
	{
		return getLlvmType(e)->isArrayTy();
	}
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











