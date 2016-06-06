// codegen.h
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once
#include "ast.h"
#include "typeinfo.h"

#include "errors.h"

#include <vector>
#include <map>

#include "ir/irbuilder.h"

enum class SymbolType
{
	Generic,
	Function,
	Variable,
	Type
};

namespace GenError
{
	void unknownSymbol(Codegen::CodegenInstance* cgi, Ast::Expr* e, std::string symname, SymbolType st) __attribute__((noreturn));
	void duplicateSymbol(Codegen::CodegenInstance* cgi, Ast::Expr* e, std::string symname, SymbolType st) __attribute__((noreturn));
	void noOpOverload(Codegen::CodegenInstance* cgi, Ast::Expr* e, std::string type, Ast::ArithmeticOp op) __attribute__((noreturn));
	void invalidAssignment(Codegen::CodegenInstance* cgi, Ast::Expr* e, fir::Value* a, fir::Value* b) __attribute__((noreturn));
	void invalidAssignment(Codegen::CodegenInstance* cgi, Ast::Expr* e, fir::Type* a, fir::Type* b) __attribute__((noreturn));
	void nullValue(Codegen::CodegenInstance* cgi, Ast::Expr* e, int funcArgument = -1) __attribute__((noreturn));

	void invalidInitialiser(Codegen::CodegenInstance* cgi, Ast::Expr* e, std::string name,
		std::vector<fir::Value*> args) __attribute__((noreturn));

	void expected(Codegen::CodegenInstance* cgi, Ast::Expr* e, std::string exp) __attribute__((noreturn));
	void noSuchMember(Codegen::CodegenInstance* cgi, Ast::Expr* e, std::string type, std::string member);
	void noFunctionTakingParams(Codegen::CodegenInstance* cgi, Ast::Expr* e, std::string type, std::string name, std::deque<Ast::Expr*> ps);

	void printContext(std::string file, uint64_t line, uint64_t col, uint64_t len);
	void printContext(Ast::Expr* e);
}




namespace Codegen
{
	struct DependencyGraph;

	struct CodegenInstance
	{
		Ast::Root* rootNode;
		fir::Module* module;
		std::deque<SymTab_t> symTabStack;
		fir::ExecutionTarget* execTarget;

		std::deque<std::string> namespaceStack;
		std::deque<BracedBlockScope> blockStack;
		std::deque<Ast::Class*> nestedTypeStack;
		std::deque<Ast::NamespaceDecl*> usingNamespaces;
		std::deque<std::map<std::string, fir::Type*>> instantiatedGenericTypeStack;

		TypeMap_t typeMap;

		// custom operator stuff
		std::map<Ast::ArithmeticOp, std::pair<std::string, int>> customOperatorMap;
		std::map<std::string, Ast::ArithmeticOp> customOperatorMapRev;

		std::deque<Ast::Func*> funcScopeStack;

		fir::IRBuilder builder = fir::IRBuilder(fir::getDefaultFTContext());

		DependencyGraph* dependencyGraph = 0;


		struct
		{
			std::map<fir::Value*, fir::Function*> funcs;
			std::map<fir::Value*, fir::Value*> values;

			std::map<fir::Value*, std::pair<int, fir::Value*>> tupleInitVals;
			std::map<fir::Value*, std::pair<int, fir::Function*>> tupleInitFuncs;

		} globalConstructors;

		void addGlobalConstructor(std::string name, fir::Function* constructor);
		void addGlobalConstructor(fir::Value* ptr, fir::Function* constructor);
		void addGlobalConstructedValue(fir::Value* ptr, fir::Value* val);

		void addGlobalTupleConstructedValue(fir::Value* ptr, int index, fir::Value* val);
		void addGlobalTupleConstructor(fir::Value* ptr, int index, fir::Function* func);

		void finishGlobalConstructors();






		// "block" scopes, ie. breakable bodies (loops)
		void pushBracedBlock(Ast::BreakableBracedBlock* block, fir::IRBlock* body, fir::IRBlock* after);
		BracedBlockScope* getCurrentBracedBlockScope();

		Ast::Func* getCurrentFunctionScope();
		void setCurrentFunctionScope(Ast::Func* f);
		void clearCurrentFunctionScope();

		void popBracedBlock();

		// normal scopes, ie. variable scopes within braces
		void pushScope();
		SymTab_t& getSymTab();
		bool isDuplicateSymbol(const std::string& name);
		fir::Value* getSymInst(Ast::Expr* user, const std::string& name);
		SymbolPair_t* getSymPair(Ast::Expr* user, const std::string& name);
		Ast::VarDecl* getSymDecl(Ast::Expr* user, const std::string& name);
		void addSymbol(std::string name, fir::Value* ai, Ast::VarDecl* vardecl);
		void popScope();
		void clearScope();

		// function scopes: namespaces, nested functions.
		void pushNamespaceScope(std::string namespc, bool doFuncTree = true);
		void clearNamespaceScope();
		void popNamespaceScope();

		void addPublicFunc(FuncPair_t func);
		void addFunctionToScope(FuncPair_t func, FunctionTree* root = 0);
		void removeFunctionFromScope(FuncPair_t func);
		void addNewType(fir::Type* ltype, Ast::StructBase* atype, TypeKind e);

		FunctionTree* getCurrentFuncTree(std::deque<std::string>* nses = 0, FunctionTree* root = 0);
		FunctionTree* cloneFunctionTree(FunctionTree* orig, bool deep);
		void cloneFunctionTree(FunctionTree* orig, FunctionTree* clone, bool deep);

		// generic type 'scopes': contains a map resolving generic type names (K, T, U etc) to
		// legitimate, fir::Type* things.

		void pushGenericTypeStack();
		void pushGenericType(std::string id, fir::Type* type);
		fir::Type* resolveGenericType(std::string id);
		void popGenericTypeStack();


		void pushNestedTypeScope(Ast::Class* nest);
		void popNestedTypeScope();


		std::deque<std::string> getFullScope();
		std::pair<TypePair_t*, int> findTypeInFuncTree(std::deque<std::string> scope, std::string name);

		std::deque<std::string> unwrapNamespacedType(std::string raw);



		bool isDuplicateFuncDecl(Ast::FuncDecl* decl);
		bool isValidFuncOverload(FuncPair_t fp, std::deque<Ast::Expr*> params, int* castingDistance, bool exactMatch);

		std::deque<FuncPair_t> resolveFunctionName(std::string basename, std::deque<Ast::Func*>* bodiesFound = 0);
		Resolved_t resolveFunctionFromList(Ast::Expr* user, std::deque<FuncPair_t> list, std::string basename,
			std::deque<Ast::Expr*> params, bool exactMatch = false);

		Resolved_t resolveFunction(Ast::Expr* user, std::string basename, std::deque<Ast::Expr*> params, bool exactMatch = false);

		fir::Function* getDefaultConstructor(Ast::Expr* user, fir::Type* ptrType, Ast::StructBase* sb);


		void removeType(std::string name);
		TypePair_t* getType(std::string name);
		TypePair_t* getType(fir::Type* type);
		FuncPair_t* getOrDeclareLibCFunc(std::string name);




		// fir::Types for non-primitive (POD) builtin types (string)
		void applyExtensionToStruct(std::string extName);

		fir::Type* getExprType(Ast::Expr* expr, bool allowFail = false, bool setInferred = true);
		fir::Type* getExprType(Ast::Expr* expr, Resolved_t preResolvedFn, bool allowFail = false, bool setInferred = true);

		fir::Type* getExprTypeFromStringType(Ast::Expr* user, Ast::ExprType type, bool allowFail = false);

		fir::Value* autoCastType(fir::Type* target, fir::Value* right, fir::Value* rhsPtr = 0, int* distance = 0)
		__attribute__ ((warn_unused_result));

		fir::Value* autoCastType(fir::Value* left, fir::Value* right, fir::Value* rhsPtr = 0, int* distance = 0)
		__attribute__ ((warn_unused_result));


		int getAutoCastDistance(fir::Type* from, fir::Type* to);

		bool isPtr(Ast::Expr* e);
		bool isEnum(Ast::ExprType type);
		bool isEnum(fir::Type* type);
		bool isArrayType(Ast::Expr* e);
		bool isSignedType(Ast::Expr* e);
		bool isBuiltinType(Ast::Expr* e);
		bool isIntegerType(Ast::Expr* e);
		bool isBuiltinType(fir::Type* e);
		bool isTypeAlias(Ast::ExprType type);
		bool isTypeAlias(fir::Type* type);
		bool isAnyType(fir::Type* type);
		bool isTupleType(fir::Type* type);
		bool areEqualTypes(fir::Type* a, fir::Type* b);

		bool isDuplicateType(std::string name);

		fir::Value* lastMinuteUnwrapType(Ast::Expr* user, fir::Value* alloca);

		std::string mangleLlvmType(fir::Type* t);

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
		std::string mangleFunctionName(std::string base, std::deque<fir::Type*> args);
		std::string mangleFunctionName(std::string base, std::deque<std::string> args);
		std::string mangleFunctionName(std::string base, std::deque<Ast::VarDecl*> args);
		std::string mangleGenericFunctionName(std::string base, std::deque<Ast::VarDecl*> args);


		std::string getReadableType(Ast::Expr* expr);
		std::string getReadableType(fir::Type* type);
		std::string getReadableType(fir::Value* val);


		fir::Value* allocateInstanceInBlock(Ast::VarDecl* var);
		fir::Value* allocateInstanceInBlock(fir::Type* type, std::string name = "");

		std::string printAst(Ast::Expr*);

		fir::Type* parseAndGetOrInstantiateType(Ast::Expr* user, std::string type, bool allowFail = false);

		std::pair<fir::Type*, Ast::Result_t> resolveStaticDotOperator(Ast::MemberAccess* ma, bool actual = true);

		Ast::Func* getFunctionFromMemberFuncCall(Ast::Class* str, Ast::FuncCall* fc);
		Ast::Expr* getStructMemberByName(Ast::StructBase* str, Ast::VarRef* var);

		Ast::Result_t getStaticVariable(Ast::Expr* user, Ast::Class* str, std::string name);


		Ast::Result_t getEnumerationCaseValue(Ast::Expr* user, TypePair_t* enr, std::string casename, bool actual = true);
		Ast::Result_t getEnumerationCaseValue(Ast::Expr* lhs, Ast::Expr* rhs, bool actual = true);



		Ast::Result_t doBinOpAssign(Ast::Expr* user, Ast::Expr* l, Ast::Expr* r, Ast::ArithmeticOp op, fir::Value* lhs, fir::Value* ref, fir::Value* rhs, fir::Value* rhsPtr);

		Ast::Result_t doTupleAccess(fir::Value* selfPtr, Ast::Number* num, bool createPtr);

		Ast::Result_t assignValueToAny(fir::Value* lhsPtr, fir::Value* rhs, fir::Value* rhsPtr);
		Ast::Result_t extractValueFromAny(fir::Type* type, fir::Value* ptr);
		Ast::Result_t makeAnyFromValue(fir::Value* value);

		Ast::Result_t createStringFromInt8Ptr(fir::StructType* stringType, fir::Value* int8ptr);

		fir::Function* tryResolveAndInstantiateGenericFunction(Ast::FuncCall* fc);

		fir::FTContext* getContext();
		fir::Value* getDefaultValue(Ast::Expr* e);
		bool verifyAllPathsReturn(Ast::Func* func, size_t* stmtCounter, bool checkType, fir::Type* retType = 0);

		fir::Type* getExprTypeOfBuiltin(std::string type);
		Ast::ArithmeticOp determineArithmeticOp(std::string ch);
		fir::Instruction getBinaryOperator(Ast::ArithmeticOp op, bool isSigned, bool isFP);
		fir::Function* getStructInitialiser(Ast::Expr* user, TypePair_t* pair, std::vector<fir::Value*> args);
		Ast::Result_t doPointerArithmetic(Ast::ArithmeticOp op, fir::Value* lhs, fir::Value* lhsptr, fir::Value* rhs);
		Ast::Result_t callTypeInitialiser(TypePair_t* tp, Ast::Expr* user, std::vector<fir::Value*> args);

		// <isBinOp, isInType, isPrefix, needsSwap, needsNOT, needsAssign, opFunc, assignFunc>
		std::tuple<bool, bool, bool, bool, bool, bool, fir::Function*, fir::Function*>
		getOperatorOverload(Ast::Expr* u, Ast::ArithmeticOp op, fir::Type* lhs, fir::Type* rhs);

		Ast::Result_t callOperatorOverload(std::tuple<bool, bool, bool, bool, bool, bool, fir::Function*, fir::Function*> data, fir::Value* lhs, fir::Value* lhsRef, fir::Value* rhs, fir::Value* rhsRef, Ast::ArithmeticOp op);


		Ast::Expr* cloneAST(Ast::Expr* e);


		~CodegenInstance();
	};

	std::string unwrapPointerType(std::string type, int* indirections);

	void doCodegen(std::string filename, Ast::Root* root, CodegenInstance* cgi);
}



