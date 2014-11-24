// codegen.h
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once
#include "ast.h"
#include "llvm_all.h"


void __error_gen(Ast::Expr* relevantast, const char* msg, const char* type, bool ex, va_list ap);

void error(const char* msg, ...);
void error(Ast::Expr* e, const char* msg, ...);

void int_error(const char* msg, ...);
void int_error(Ast::Expr* e, const char* msg, ...);

void warn(const char* msg, ...);
void warn(Ast::Expr* e, const char* msg, ...);

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
	typedef std::pair<llvm::Function*, Ast::FuncDecl*> FuncPair_t;
	typedef std::map<std::string, FuncPair_t> FuncMap_t;

	class CodegenInstance
	{
		public:
			Ast::Root* rootNode;
			llvm::Module* mainModule;
			llvm::FunctionPassManager* Fpm;
			std::deque<SymTab_t*> symTabStack;
			llvm::ExecutionEngine* execEngine;
			std::deque<FuncMap_t*> funcTabStack;
			std::deque<TypeMap_t*> visibleTypes;
			llvm::IRBuilder<> mainBuilder = llvm::IRBuilder<>(llvm::getGlobalContext());


			void popScope();
			void pushScope();
			SymTab_t& getSymTab();
			Ast::Root* getRootAST();
			bool isPtr(Ast::Expr* e);
			TypeMap_t& getVisibleTypes();
			bool isArrayType(Ast::Expr* e);
			llvm::LLVMContext& getContext();
			bool isSignedType(Ast::Expr* e);
			bool isBuiltinType(Ast::Expr* e);
			bool isIntegerType(Ast::Expr* e);
			FuncMap_t& getVisibleFuncDecls();
			TypePair_t* getType(std::string name);
			bool isDuplicateType(std::string name);
			llvm::Type* getLlvmType(Ast::Expr* expr);
			bool isDuplicateFuncDecl(std::string name);
			llvm::Value* getDefaultValue(Ast::Expr* e);
			Ast::VarType determineVarType(Ast::Expr* e);
			Ast::FuncDecl* getFuncDecl(std::string name);
			std::string getReadableType(Ast::Expr* expr);
			void pushScope(SymTab_t* tab, TypeMap_t* tp);
			std::string getReadableType(llvm::Type* type);
			bool isDuplicateSymbol(const std::string& name);
			llvm::Type* unwrapPointerType(std::string type);
			llvm::Value* getSymInst(const std::string& name);
			llvm::Type* getLlvmTypeOfBuiltin(Ast::VarType t);
			SymbolPair_t* getSymPair(const std::string& name);
			Ast::VarDecl* getSymDecl(const std::string& name);
			Ast::ArithmeticOp determineArithmeticOp(std::string ch);
			std::string mangleName(Ast::Struct* s, std::string orig);
			std::string unmangleName(Ast::Struct* s, std::string orig);
			Ast::Expr* autoCastType(Ast::Expr* left, Ast::Expr* right);
			void pushScope(SymTab_t* tab, TypeMap_t* tp, FuncMap_t* fm);
			std::string mangleName(std::string base, std::deque<Ast::Expr*> args);
			std::string mangleName(std::string base, std::deque<Ast::VarDecl*> args);
			llvm::AllocaInst* allocateInstanceInBlock(llvm::Function* func, Ast::VarDecl* var);
			llvm::AllocaInst* allocateInstanceInBlock(llvm::Function* func, llvm::Type* type, std::string name);
			Ast::ValPtr_p callOperatorOnStruct(TypePair_t* pair, llvm::Value* self, Ast::ArithmeticOp op, llvm::Value* val);
	};

	llvm::Module* doCodegen(std::string filename, Ast::Root* root, CodegenInstance* cgi);
}








