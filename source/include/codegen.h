// codegen.h
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once
#include "ast.h"
#include "llvm_all.h"



extern void error(const char* msg, ...);

namespace Codegen
{
	enum class ExprType
	{
		Struct,
		Func
	};

	typedef std::pair<llvm::AllocaInst*, Ast::VarDecl*> SymbolPair_t;
	typedef std::map<std::string, SymbolPair_t> SymTab_t;
	typedef std::pair<Ast::Expr*, ExprType> TypedExpr_t;
	typedef std::pair<llvm::Type*, TypedExpr_t> TypePair_t;
	typedef std::map<std::string, TypePair_t> TypeMap_t;


	extern llvm::Module* mainModule;
	extern llvm::IRBuilder<> mainBuilder;
	extern llvm::FunctionPassManager* Fpm;
	extern std::deque<SymTab_t*> symTabStack;
	extern llvm::ExecutionEngine* execEngine;
	extern std::deque<TypeMap_t*> visibleTypes;
	extern std::map<std::string, Ast::FuncDecl*> funcTable;



	void popScope();
	void pushScope();
	SymTab_t& getSymTab();
	bool isPtr(Ast::Expr* e);
	TypeMap_t& getVisibleTypes();
	llvm::LLVMContext& getContext();
	bool isSignedType(Ast::Expr* e);
	bool isBuiltinType(Ast::Expr* e);
	bool isIntegerType(Ast::Expr* e);
	TypePair_t* getType(std::string name);
	bool isDuplicateType(std::string name);
	llvm::Type* getLlvmType(Ast::Expr* expr);
	llvm::Value* getDefaultValue(Ast::Expr* e);
	Ast::VarType determineVarType(Ast::Expr* e);
	std::string getReadableType(Ast::Expr* expr);
	void pushScope(SymTab_t* tab, TypeMap_t* tp);
	std::string getReadableType(llvm::Type* type);
	bool isDuplicateSymbol(const std::string& name);
	llvm::Value* getSymInst(const std::string& name);
	SymbolPair_t* getSymPair(const std::string& name);
	Ast::VarDecl* getSymDecl(const std::string& name);
	std::string mangleName(Ast::Struct* s, std::string orig);
	std::string unmangleName(Ast::Struct* s, std::string orig);
	Ast::Expr* autoCastType(Ast::Expr* left, Ast::Expr* right);
	llvm::AllocaInst* allocateInstanceInBlock(llvm::Function* func, Ast::VarDecl* var);
	llvm::AllocaInst* allocateInstanceInBlock(llvm::Function* func, llvm::Type* type, std::string name);

}








