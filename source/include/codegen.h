// codegen.h
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once
#include "ast.h"
#include "llvm_all.h"


void __error_gen(Ast::Expr* relevantast, const char* msg, const char* type, bool ex, va_list ap);

void error(const char* msg, ...) __attribute__((noreturn));
void error(Ast::Expr* e, const char* msg, ...) __attribute__((noreturn));

void warn(const char* msg, ...);
void warn(Ast::Expr* e, const char* msg, ...);


enum class SymbolType
{
	Generic,
	Function,
	Variable,
	Type
};

namespace GenError
{
	void unknownSymbol(Ast::Expr* e, std::string symname, SymbolType st) __attribute__((noreturn));
	void useAfterFree(Ast::Expr* e, std::string symname) __attribute__((noreturn));
	void duplicateSymbol(Ast::Expr* e, std::string symname, SymbolType st) __attribute__((noreturn));
	void noOpOverload(Ast::Expr* e, std::string type, Ast::ArithmeticOp op) __attribute__((noreturn));
	void invalidAssignment(Ast::Expr* e, llvm::Value* a, llvm::Value* b) __attribute__((noreturn));
	void invalidAssignment(Ast::Expr* e, llvm::Type* a, llvm::Type* b) __attribute__((noreturn));
	void invalidInitialiser(Ast::Expr* e, Ast::Struct* str, std::vector<llvm::Value*> args) __attribute__((noreturn));
}









namespace Codegen
{
	enum class ExprType
	{
		Struct,
		Func
	};

	enum class SymbolValidity
	{
		Valid,
		UseAfterDealloc
	};

	struct WrappedType
	{
		WrappedType(llvm::Type* t, bool us) : ltype(t), isUnsigned(us) { }
		llvm::Type* ltype;
		bool isUnsigned;
	};

	typedef std::pair<llvm::AllocaInst*, SymbolValidity> SymbolValidity_t;
	typedef std::pair<SymbolValidity_t, Ast::VarDecl*> SymbolPair_t;
	typedef std::map<std::string, SymbolPair_t> SymTab_t;
	typedef std::pair<Ast::Expr*, ExprType> TypedExpr_t;
	typedef std::pair<llvm::Type*, TypedExpr_t> TypePair_t;
	typedef std::map<std::string, TypePair_t> TypeMap_t;
	typedef std::pair<llvm::Function*, Ast::FuncDecl*> FuncPair_t;
	typedef std::map<std::string, FuncPair_t> FuncMap_t;
	typedef std::pair<std::string, FuncMap_t> NamespacePair_t;
	typedef std::pair<Ast::BreakableBracedBlock*, std::pair<llvm::BasicBlock*, llvm::BasicBlock*>> BracedBlockScope;

	struct CodegenInstance
	{
		Ast::Root* rootNode;
		llvm::Module* mainModule;
		llvm::FunctionPassManager* Fpm;
		std::deque<SymTab_t*> symTabStack;
		llvm::ExecutionEngine* execEngine;
		std::deque<TypeMap_t*> visibleTypes;
		std::deque<BracedBlockScope> blockStack;
		std::deque<NamespacePair_t*> funcTabStack;
		llvm::IRBuilder<> mainBuilder = llvm::IRBuilder<>(llvm::getGlobalContext());

		// "block" scopes, ie. breakable bodies (loops)
		void pushBracedBlock(Ast::BreakableBracedBlock* block, llvm::BasicBlock* body, llvm::BasicBlock* after);
		BracedBlockScope* getCurrentBracedBlockScope();
		void popBracedBlock();

		// normal scopes, ie. variable scopes within braces
		void pushScope();
		void pushScope(SymTab_t* tab, TypeMap_t* tp);
		SymTab_t& getSymTab();
		bool isDuplicateSymbol(const std::string& name);
		llvm::Value* getSymInst(Ast::Expr* user, const std::string& name);
		SymbolPair_t* getSymPair(Ast::Expr* user, const std::string& name);
		Ast::VarDecl* getSymDecl(Ast::Expr* user, const std::string& name);
		void addSymbol(std::string name, llvm::AllocaInst* ai, Ast::VarDecl* vardecl);
		void popScope();

		// function scopes: namespaces, nested functions.
		void pushFuncScope(std::string namespc);
		void addFunctionToScope(std::string name, FuncPair_t func);
		bool isDuplicateFuncDecl(std::string name);
		FuncPair_t* getDeclaredFunc(std::string name);
		void popFuncScope();

		Ast::Root* getRootAST();
		bool isPtr(Ast::Expr* e);
		bool isArrayType(Ast::Expr* e);
		llvm::LLVMContext& getContext();
		bool isSignedType(Ast::Expr* e);
		bool isBuiltinType(Ast::Expr* e);
		bool isIntegerType(Ast::Expr* e);
		TypePair_t* getType(std::string name);
		bool isDuplicateType(std::string name);
		llvm::Type* getLlvmType(Ast::Expr* expr);
		llvm::Value* getDefaultValue(Ast::Expr* e);
		void verifyAllPathsReturn(Ast::Func* func);
		Ast::VarType determineVarType(Ast::Expr* e);
		std::string getReadableType(Ast::Expr* expr);
		std::string getReadableType(llvm::Type* type);
		llvm::Type* unwrapPointerType(std::string type);
		llvm::Type* getLlvmTypeOfBuiltin(Ast::VarType t);
		Ast::ArithmeticOp determineArithmeticOp(std::string ch);
		std::string mangleName(Ast::Struct* s, std::string orig);
		std::string unmangleName(Ast::Struct* s, std::string orig);
		Ast::Expr* autoCastType(Ast::Expr* left, Ast::Expr* right);
		void autoCastLlvmType(llvm::Value*& left, llvm::Value*& right);
		void addNewType(llvm::Type* ltype, Ast::Struct* atype, ExprType e);
		std::string unwrapPointerType(std::string type, int* indirections);
		std::string mangleName(std::string base, std::deque<Ast::Expr*> args);
		std::string mangleName(std::string base, std::deque<llvm::Type*> args);
		std::string mangleName(std::string base, std::deque<Ast::VarDecl*> args);
		std::string mangleCppName(std::string base, std::deque<Ast::Expr*> args);
		std::string mangleCppName(std::string base, std::deque<Ast::VarDecl*> args);
		llvm::AllocaInst* allocateInstanceInBlock(llvm::Function* func, Ast::VarDecl* var);
		llvm::Instruction::BinaryOps getBinaryOperator(Ast::ArithmeticOp op, bool isSigned, bool isFP);
		llvm::AllocaInst* allocateInstanceInBlock(llvm::Function* func, llvm::Type* type, std::string name);
		llvm::Function* getStructInitialiser(Ast::Expr* user, TypePair_t* pair, std::vector<llvm::Value*> args);
		Ast::Result_t doPointerArithmetic(Ast::ArithmeticOp op, llvm::Value* lhs, llvm::Value* lhsptr, llvm::Value* rhs);
		Ast::Result_t doBinOpAssign(Ast::Expr* user, Ast::Expr* l, Ast::Expr* r, Ast::ArithmeticOp op, llvm::Value* lhs, llvm::Value* ref, llvm::Value* rhs);
		Ast::Result_t callOperatorOnStruct(TypePair_t* pair, llvm::Value* self, Ast::ArithmeticOp op, llvm::Value* val, bool fail = true);
	};

	void doCodegen(std::string filename, Ast::Root* root, CodegenInstance* cgi);
	void writeBitcode(std::string filename, CodegenInstance* cgi);

}








