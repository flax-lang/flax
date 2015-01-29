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
	typedef std::pair<std::string, FuncMap_t> NamespacePair_t;
	typedef std::pair<Ast::BreakableClosure*, std::pair<llvm::BasicBlock*, llvm::BasicBlock*>> ClosureScope;

	class CodegenInstance
	{
		public:
			Ast::Root* rootNode;
			llvm::Module* mainModule;
			llvm::FunctionPassManager* Fpm;
			std::deque<SymTab_t*> symTabStack;
			llvm::ExecutionEngine* execEngine;
			std::deque<TypeMap_t*> visibleTypes;
			std::deque<ClosureScope> closureStack;
			std::deque<NamespacePair_t*> funcTabStack;
			llvm::IRBuilder<> mainBuilder = llvm::IRBuilder<>(llvm::getGlobalContext());

			// "closure" scopes, ie. breakable bodies (loops)
			void pushClosure(Ast::BreakableClosure* closure, llvm::BasicBlock* body, llvm::BasicBlock* after);
			ClosureScope* getCurrentClosureScope();
			void popClosure();

			// normal scopes, ie. variable scopes within braces
			void pushScope();
			void pushScope(SymTab_t* tab, TypeMap_t* tp);
			SymTab_t& getSymTab();
			bool isDuplicateSymbol(const std::string& name);
			llvm::Value* getSymInst(const std::string& name);
			SymbolPair_t* getSymPair(const std::string& name);
			Ast::VarDecl* getSymDecl(const std::string& name);
			void popScope();

			// function scopes: namespaces, nested functions.
			void pushFuncScope(std::string namespc);
			void addFunctionToScope(std::string name, FuncPair_t func);
			bool isDuplicateFuncDecl(std::string name);
			FuncPair_t* getDeclaredFunc(std::string name);
			void popFuncScope();


			Ast::Root* getRootAST();
			bool isPtr(Ast::Expr* e);
			TypeMap_t& getVisibleTypes();
			bool isArrayType(Ast::Expr* e);
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
			std::string getReadableType(llvm::Type* type);
			llvm::Type* unwrapPointerType(std::string type);
			llvm::Type* getLlvmTypeOfBuiltin(Ast::VarType t);
			Ast::ArithmeticOp determineArithmeticOp(std::string ch);
			std::string mangleName(Ast::Struct* s, std::string orig);
			std::string unmangleName(Ast::Struct* s, std::string orig);
			Ast::Expr* autoCastType(Ast::Expr* left, Ast::Expr* right);
			std::string mangleName(std::string base, std::deque<Ast::Expr*> args);
			std::string mangleName(std::string base, std::deque<Ast::VarDecl*> args);
			llvm::AllocaInst* allocateInstanceInBlock(llvm::Function* func, Ast::VarDecl* var);
			llvm::Instruction::BinaryOps getBinaryOperator(Ast::ArithmeticOp op, bool isSigned);
			llvm::AllocaInst* allocateInstanceInBlock(llvm::Function* func, llvm::Type* type, std::string name);
			Ast::Result_t callOperatorOnStruct(TypePair_t* pair, llvm::Value* self, Ast::ArithmeticOp op, llvm::Value* val);
	};

	void doCodegen(std::string filename, Ast::Root* root, CodegenInstance* cgi);
	void writeBitcode(std::string filename, CodegenInstance* cgi);
}








