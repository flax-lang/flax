// codegen.h
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once
#include "ast.h"
#include "typeinfo.h"

#include <vector>
#include <map>

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/GlobalValue.h"
#include "llvm/PassManager.h"

enum class SymbolType
{
	Generic,
	Function,
	Variable,
	Type
};

namespace llvm
{
	class Module;
	class ExecutionEngine;
	class GlobalVariable;
	class AllocaInst;
	class GlobalValue;
	class LLVMContext;
	class Instruction;
}

namespace GenError
{
	void unknownSymbol(Codegen::CodegenInstance* cgi, Ast::Expr* e, std::string symname, SymbolType st) __attribute__((noreturn));
	void useAfterFree(Codegen::CodegenInstance* cgi, Ast::Expr* e, std::string symname);
	void duplicateSymbol(Codegen::CodegenInstance* cgi, Ast::Expr* e, std::string symname, SymbolType st) __attribute__((noreturn));
	void noOpOverload(Codegen::CodegenInstance* cgi, Ast::Expr* e, std::string type, Ast::ArithmeticOp op) __attribute__((noreturn));
	void invalidAssignment(Codegen::CodegenInstance* cgi, Ast::Expr* e, llvm::Value* a, llvm::Value* b) __attribute__((noreturn));
	void invalidAssignment(Codegen::CodegenInstance* cgi, Ast::Expr* e, llvm::Type* a, llvm::Type* b) __attribute__((noreturn));
	void nullValue(Codegen::CodegenInstance* cgi, Ast::Expr* e, int funcArgument = -1) __attribute__((noreturn));

	void invalidInitialiser(Codegen::CodegenInstance* cgi, Ast::Expr* e, std::string name,
		std::vector<llvm::Value*> args) __attribute__((noreturn));

	void expected(Codegen::CodegenInstance* cgi, Ast::Expr* e, std::string exp) __attribute__((noreturn));
	void noSuchMember(Codegen::CodegenInstance* cgi, Ast::Expr* e, std::string type, std::string member);
	void noFunctionTakingParams(Codegen::CodegenInstance* cgi, Ast::Expr* e, std::string type, std::string name, std::deque<Ast::Expr*> ps);

	void printContext(std::vector<std::string> lines, uint64_t line, uint64_t col);
	void printContext(Codegen::CodegenInstance* cgi, uint64_t line, uint64_t col);
	void printContext(Codegen::CodegenInstance* cgi, Ast::Expr* e);
}

void __error_gen(std::vector<std::string> lines, uint64_t line, uint64_t col, const char* file, const char* msg, const char* type,
	bool doExit, va_list ap);

void error(const char* msg, ...) __attribute__((noreturn, format(printf, 1, 2)));
void error(Ast::Expr* e, const char* msg, ...) __attribute__((noreturn, format(printf, 2, 3)));
void error(Codegen::CodegenInstance* cgi, Ast::Expr* e, const char* msg, ...) __attribute__((noreturn, format(printf, 3, 4)));

void warn(const char* msg, ...) __attribute__((format(printf, 1, 2)));
void warn(Ast::Expr* e, const char* msg, ...) __attribute__((format(printf, 2, 3)));
void warn(Codegen::CodegenInstance* cgi, Ast::Expr* e, const char* msg, ...) __attribute__((format(printf, 3, 4)));

void info(const char* msg, ...) __attribute__((format(printf, 1, 2)));
void info(Ast::Expr* e, const char* msg, ...) __attribute__((format(printf, 2, 3)));
void info(Codegen::CodegenInstance* cgi, Ast::Expr* e, const char* msg, ...) __attribute__((format(printf, 3, 4)));



#define __nothing
#define iceAssert(x)		((x) ? (void) (0) : error("Compiler assertion at %s:%d, cause:\n'%s' evaluated to false", __FILE__, __LINE__, #x))




namespace Codegen
{
	struct CodegenInstance
	{
		// todo: hack
		bool isStructCodegen = false;

		Ast::Root* rootNode;
		llvm::Module* module;
		llvm::FunctionPassManager* Fpm;
		std::deque<SymTab_t> symTabStack;
		llvm::ExecutionEngine* execEngine;

		std::deque<std::string> namespaceStack;
		std::deque<BracedBlockScope> blockStack;
		std::deque<Ast::Class*> nestedTypeStack;
		std::deque<Ast::NamespaceDecl*> usingNamespaces;
		std::deque<std::map<std::string, llvm::Type*>> instantiatedGenericTypeStack;

		std::vector<std::string> rawLines;

		TypeMap_t typeMap;

		// custom operator stuff
		std::map<Ast::ArithmeticOp, std::pair<std::string, int>> customOperatorMap;
		std::map<std::string, Ast::ArithmeticOp> customOperatorMapRev;

		std::deque<Ast::Func*> funcScopeStack;

		llvm::IRBuilder<> builder = llvm::IRBuilder<>(llvm::getGlobalContext());


		struct
		{
			std::map<llvm::Value*, llvm::Function*> funcs;
			std::map<llvm::Value*, llvm::Value*> values;

			std::map<llvm::Value*, std::pair<int, llvm::Value*>> tupleInitVals;
			std::map<llvm::Value*, std::pair<int, llvm::Function*>> tupleInitFuncs;

		} globalConstructors;

		void addGlobalConstructor(std::string name, llvm::Function* constructor);
		void addGlobalConstructor(llvm::Value* ptr, llvm::Function* constructor);
		void addGlobalConstructedValue(llvm::Value* ptr, llvm::Value* val);

		void addGlobalTupleConstructedValue(llvm::Value* ptr, int index, llvm::Value* val);
		void addGlobalTupleConstructor(llvm::Value* ptr, int index, llvm::Function* func);

		void finishGlobalConstructors();






		// "block" scopes, ie. breakable bodies (loops)
		void pushBracedBlock(Ast::BreakableBracedBlock* block, llvm::BasicBlock* body, llvm::BasicBlock* after);
		BracedBlockScope* getCurrentBracedBlockScope();

		Ast::Func* getCurrentFunctionScope();
		void setCurrentFunctionScope(Ast::Func* f);
		void clearCurrentFunctionScope();

		void popBracedBlock();

		// normal scopes, ie. variable scopes within braces
		void pushScope();
		SymTab_t& getSymTab();
		bool isDuplicateSymbol(const std::string& name);
		llvm::Value* getSymInst(Ast::Expr* user, const std::string& name);
		SymbolPair_t* getSymPair(Ast::Expr* user, const std::string& name);
		Ast::VarDecl* getSymDecl(Ast::Expr* user, const std::string& name);
		void addSymbol(std::string name, llvm::Value* ai, Ast::VarDecl* vardecl);
		void popScope();
		void clearScope();

		// function scopes: namespaces, nested functions.
		void pushNamespaceScope(std::string namespc, bool doFuncTree = true);
		void clearNamespaceScope();
		void popNamespaceScope();

		void addFunctionToScope(FuncPair_t func);
		void removeFunctionFromScope(FuncPair_t func);
		void addNewType(llvm::Type* ltype, Ast::StructBase* atype, TypeKind e);

		FunctionTree* getCurrentFuncTree(std::deque<std::string>* nses = 0, FunctionTree* root = 0);
		FunctionTree* cloneFunctionTree(FunctionTree* orig, bool deep);
		void cloneFunctionTree(FunctionTree* orig, FunctionTree* clone, bool deep);

		// generic type 'scopes': contains a map resolving generic type names (K, T, U etc) to
		// legitimate, llvm::Type* things.

		void pushGenericTypeStack();
		void pushGenericType(std::string id, llvm::Type* type);
		llvm::Type* resolveGenericType(std::string id);
		void popGenericTypeStack();


		void pushNestedTypeScope(Ast::Class* nest);
		std::deque<std::string> getNestedTypeList();
		void popNestedTypeScope();


		std::deque<std::string> getFullScope();





		bool isDuplicateFuncDecl(Ast::FuncDecl* decl);
		bool isValidFuncOverload(FuncPair_t fp, std::deque<Ast::Expr*> params, int* castingDistance, bool exactMatch);

		std::deque<FuncPair_t> resolveFunctionName(std::string basename);
		Resolved_t resolveFunctionFromList(Ast::Expr* user, std::deque<FuncPair_t> list, std::string basename,
			std::deque<Ast::Expr*> params, bool exactMatch = false);

		Resolved_t resolveFunction(Ast::Expr* user, std::string basename, std::deque<Ast::Expr*> params, bool exactMatch = false);
		void addPublicFunc(FuncPair_t fp);

		llvm::Function* getDefaultConstructor(Ast::Expr* user, llvm::Type* ptrType, Ast::StructBase* sb);

		std::deque<Ast::NamespaceDecl*> resolveNamespace(std::string name);


		void removeType(std::string name);
		TypePair_t* getType(std::string name);
		TypePair_t* getType(llvm::Type* type);
		FuncPair_t* getOrDeclareLibCFunc(std::string name);




		// llvm::Types for non-primitive (POD) builtin types (string)
		void applyExtensionToStruct(std::string extName);

		llvm::Type* getLlvmType(Ast::Expr* expr, bool allowFail = false, bool setInferred = true);
		llvm::Type* getLlvmType(Ast::Expr* expr, Resolved_t preResolvedFn, bool allowFail = false, bool setInferred = true);

		llvm::Type* getLlvmTypeFromString(Ast::Expr* user, Ast::ExprType type, bool allowFail = false);
		int autoCastType(llvm::Type* target, llvm::Value*& right, llvm::Value* rhsPtr = 0);
		int autoCastType(llvm::Value* left, llvm::Value*& right, llvm::Value* rhsPtr = 0);
		int getAutoCastDistance(llvm::Type* from, llvm::Type* to);

		bool isPtr(Ast::Expr* e);
		bool isEnum(Ast::ExprType type);
		bool isEnum(llvm::Type* type);
		bool isArrayType(Ast::Expr* e);
		bool isSignedType(Ast::Expr* e);
		bool isBuiltinType(Ast::Expr* e);
		bool isIntegerType(Ast::Expr* e);
		bool isBuiltinType(llvm::Type* e);
		bool isTypeAlias(Ast::ExprType type);
		bool isTypeAlias(llvm::Type* type);
		bool isAnyType(llvm::Type* type);
		bool isTupleType(llvm::Type* type);
		bool areEqualTypes(llvm::Type* a, llvm::Type* b);

		bool isDuplicateType(std::string name);

		llvm::Value* lastMinuteUnwrapType(Ast::Expr* user, llvm::Value* alloca);

		std::string mangleLlvmType(llvm::Type* t);

		std::string mangleRawNamespace(std::string original);
		std::string mangleWithNamespace(std::string original, bool isFunction = true);
		std::string mangleWithNamespace(std::string original, std::deque<std::string> ns, bool isFunction = true);

		std::string mangleMemberFunction(Ast::StructBase* s, std::string orig, std::deque<Ast::Expr*> args);
		std::string mangleMemberFunction(Ast::StructBase* s, std::string orig, std::deque<Ast::Expr*> args, std::deque<std::string> ns);
		std::string mangleMemberFunction(Ast::StructBase* s, std::string orig, std::deque<Ast::VarDecl*> args, std::deque<std::string> ns,
			bool isStatic = false);

		std::string mangleMemberName(Ast::StructBase* s, std::string orig);
		std::string mangleMemberName(Ast::StructBase* s, Ast::FuncCall* fc);

		std::string mangleFunctionName(std::string base, std::deque<Ast::Expr*> args);
		std::string mangleFunctionName(std::string base, std::deque<llvm::Type*> args);
		std::string mangleFunctionName(std::string base, std::deque<std::string> args);
		std::string mangleFunctionName(std::string base, std::deque<Ast::VarDecl*> args);
		std::string mangleGenericFunctionName(std::string base, std::deque<Ast::VarDecl*> args);


		std::string getReadableType(Ast::Expr* expr);
		std::string getReadableType(llvm::Type* type);
		std::string getReadableType(llvm::Value* val);


		llvm::AllocaInst* allocateInstanceInBlock(Ast::VarDecl* var);
		llvm::AllocaInst* allocateInstanceInBlock(llvm::Type* type, std::string name = "");

		std::string printAst(Ast::Expr*);

		llvm::Type* parseTypeFromString(Ast::Expr* user, std::string type, bool allowFail = false);
		std::string unwrapPointerType(std::string type, int* indirections);

		std::pair<llvm::Type*, Ast::Result_t> resolveStaticDotOperator(Ast::MemberAccess* ma, bool actual = true);

		Ast::Func* getFunctionFromMemberFuncCall(Ast::Class* str, Ast::FuncCall* fc);
		Ast::Expr* getStructMemberByName(Ast::StructBase* str, Ast::VarRef* var);

		Ast::Result_t getStaticVariable(Ast::Expr* user, Ast::Class* str, std::string name);


		Ast::Result_t getEnumerationCaseValue(Ast::Expr* user, TypePair_t* enr, std::string casename, bool actual = true);
		Ast::Result_t getEnumerationCaseValue(Ast::Expr* lhs, Ast::Expr* rhs, bool actual = true);



		Ast::Result_t doBinOpAssign(Ast::Expr* user, Ast::Expr* l, Ast::Expr* r, Ast::ArithmeticOp op, llvm::Value* lhs, llvm::Value* ref, llvm::Value* rhs, llvm::Value* rhsPtr);

		Ast::Result_t doTupleAccess(llvm::Value* selfPtr, Ast::Number* num, bool createPtr);

		Ast::Result_t assignValueToAny(llvm::Value* lhsPtr, llvm::Value* rhs, llvm::Value* rhsPtr);
		Ast::Result_t extractValueFromAny(llvm::Type* type, llvm::Value* ptr);

		Ast::Result_t createStringFromInt8Ptr(llvm::StructType* stringType, llvm::Value* int8ptr);

		llvm::Function* tryResolveAndInstantiateGenericFunction(Ast::FuncCall* fc);


		llvm::GlobalValue::LinkageTypes getFunctionDeclLinkage(Ast::FuncDecl* fd);
		Ast::Result_t generateActualFuncDecl(Ast::FuncDecl* fd, std::vector<llvm::Type*> argtypes, llvm::Type* rettype);

		Ast::Root* getRootAST();
		llvm::LLVMContext& getContext();
		llvm::Value* getDefaultValue(Ast::Expr* e);
		bool verifyAllPathsReturn(Ast::Func* func, size_t* stmtCounter, bool checkType, llvm::Type* retType = 0);

		llvm::Type* getLlvmTypeOfBuiltin(std::string type);
		Ast::ArithmeticOp determineArithmeticOp(std::string ch);
		llvm::Instruction::BinaryOps getBinaryOperator(Ast::ArithmeticOp op, bool isSigned, bool isFP);
		llvm::Function* getStructInitialiser(Ast::Expr* user, TypePair_t* pair, std::vector<llvm::Value*> args);
		Ast::Result_t doPointerArithmetic(Ast::ArithmeticOp op, llvm::Value* lhs, llvm::Value* lhsptr, llvm::Value* rhs);
		Ast::Result_t callOperatorOnStruct(Ast::Expr* user, TypePair_t* pair, llvm::Value* self, Ast::ArithmeticOp op, llvm::Value* val, bool fail = true);
		Ast::Result_t callTypeInitialiser(TypePair_t* tp, Ast::Expr* user, std::vector<llvm::Value*> args);


		Ast::Expr* cloneAST(Ast::Expr* e);


		~CodegenInstance();
	};

	void doCodegen(std::string filename, Ast::Root* root, CodegenInstance* cgi);
	void writeBitcode(std::string filename, CodegenInstance* cgi);
}



