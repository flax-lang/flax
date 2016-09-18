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

	void nullValue(Codegen::CodegenInstance* cgi, Ast::Expr* e) __attribute__((noreturn));
	void assignToImmutable(Codegen::CodegenInstance* cgi, Ast::Expr* op, Ast::Expr* value) __attribute__((noreturn));

	void invalidInitialiser(Codegen::CodegenInstance* cgi, Ast::Expr* e, std::string name,
		std::vector<fir::Value*> args) __attribute__((noreturn));

	void expected(Codegen::CodegenInstance* cgi, Ast::Expr* e, std::string exp) __attribute__((noreturn));
	void noSuchMember(Codegen::CodegenInstance* cgi, Ast::Expr* e, std::string type, std::string member);
	void noFunctionTakingParams(Codegen::CodegenInstance* cgi, Ast::Expr* e, std::string type, std::string name, std::deque<Ast::Expr*> ps);

	std::tuple<std::string, std::string, HighlightOptions> getPrettyNoSuchFunctionError(Codegen::CodegenInstance* cgi, std::deque<Ast::Expr*> args, std::deque<Codegen::FuncPair_t> cands);

	void prettyNoSuchFunctionError(Codegen::CodegenInstance* cgi, Ast::Expr* e, std::string name, std::deque<Ast::Expr*> args) __attribute__((noreturn));
}




namespace Codegen
{
	struct DependencyGraph;

	struct _OpOverloadData
	{
		bool found					= 0;

		bool isBinOp				= 0;
		bool isPrefix				= 0;
		bool needsSwap				= 0;
		bool needsNot				= 0;
		bool isMember				= 0;

		fir::Function* opFunc		= 0;
	};


	struct CodegenInstance
	{
		Ast::Root* rootNode;
		fir::Module* module;
		std::deque<SymTab_t> symTabStack;
		fir::ExecutionTarget* execTarget;

		std::deque<std::string> namespaceStack;
		std::deque<BracedBlockScope> blockStack;
		std::deque<Ast::StructBase*> nestedTypeStack;
		std::deque<Ast::NamespaceDecl*> usingNamespaces;

		// generic stuff
		std::deque<std::map<std::string, fir::Type*>> instantiatedGenericTypeStack;
		std::map<std::pair<Ast::Func*, std::map<std::string, fir::Type*>>, fir::Function*> reifiedGenericFunctions;


		TypeMap_t typeMap;

		// custom operator stuff
		std::map<Ast::ArithmeticOp, std::pair<std::string, int>> customOperatorMap;
		std::map<std::string, Ast::ArithmeticOp> customOperatorMapRev;
		std::deque<Ast::Func*> funcScopeStack;

		fir::IRBuilder builder = fir::IRBuilder(fir::getDefaultFTContext());


		struct
		{
			std::map<fir::Value*, fir::Function*> funcs;
			std::map<fir::Value*, fir::Value*> values;

			std::map<fir::Value*, std::pair<int, fir::Value*>> tupleInitVals;
			std::map<fir::Value*, std::pair<int, fir::Function*>> tupleInitFuncs;

		} globalConstructors;

		void addGlobalConstructor(Identifier name, fir::Function* constructor);
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
		void popGenericTypeStack();
		void pushGenericType(std::string id, fir::Type* type);
		void pushGenericTypeMap(std::map<std::string, fir::Type*> types);
		fir::Type* resolveGenericType(std::string id);


		bool isArithmeticOpAssignment(Ast::ArithmeticOp op);

		void pushNestedTypeScope(Ast::StructBase* nest);
		void popNestedTypeScope();


		std::deque<std::string> getFullScope();
		std::pair<TypePair_t*, int> findTypeInFuncTree(std::deque<std::string> scope, std::string name);

		std::deque<std::string> unwrapNamespacedType(std::string raw);



		bool isDuplicateFuncDecl(Ast::FuncDecl* decl);
		bool isValidFuncOverload(FuncPair_t fp, std::deque<fir::Type*> params, int* castingDistance, bool exactMatch);

		std::deque<FuncPair_t> resolveFunctionName(std::string basename);
		Resolved_t resolveFunctionFromList(Ast::Expr* user, std::deque<FuncPair_t> list, std::string basename,
			std::deque<Ast::Expr*> params, bool exactMatch = false);
		Resolved_t resolveFunctionFromList(Ast::Expr* user, std::deque<FuncPair_t> list, std::string basename,
			std::deque<fir::Type*> params, bool exactMatch = false);

		std::deque<Ast::Func*> findGenericFunctions(std::string basename);

		Resolved_t resolveFunction(Ast::Expr* user, std::string basename, std::deque<Ast::Expr*> params, bool exactMatch = false);

		fir::Function* getDefaultConstructor(Ast::Expr* user, fir::Type* ptrType, Ast::StructBase* sb);

		TypePair_t* getTypeByString(std::string name);
		TypePair_t* getType(Identifier id);
		TypePair_t* getType(fir::Type* type);
		FuncPair_t* getOrDeclareLibCFunc(std::string name);




		// fir::Type* getExprType(Ast::Expr* expr, bool allowFail = false, bool setInferred = true);
		// fir::Type* getExprType(Ast::Expr* expr, Resolved_t preResolvedFn, bool allowFail = false, bool setInferred = true);

		fir::Type* getTypeFromParserType(Ast::Expr* user, pts::Type* type, bool allowFail = false);

		fir::Value* autoCastType(fir::Type* target, fir::Value* right, fir::Value* rhsPtr = 0, int* distance = 0)
		__attribute__ ((warn_unused_result));

		fir::Value* autoCastType(fir::Value* left, fir::Value* right, fir::Value* rhsPtr = 0, int* distance = 0)
		__attribute__ ((warn_unused_result));

		int getAutoCastDistance(fir::Type* from, fir::Type* to);

		bool isEnum(fir::Type* type);
		bool isArrayType(Ast::Expr* e);
		bool isSignedType(Ast::Expr* e);
		bool isBuiltinType(Ast::Expr* e);
		bool isIntegerType(Ast::Expr* e);
		bool isBuiltinType(fir::Type* e);
		bool isTypeAlias(fir::Type* type);
		bool isAnyType(fir::Type* type);

		bool isDuplicateType(Identifier id);

		fir::Value* lastMinuteUnwrapType(Ast::Expr* user, fir::Value* alloca);

		std::string mangleGenericParameters(std::deque<Ast::VarDecl*> args);


		fir::Value* getStackAlloc(fir::Type* type, std::string name = "");
		fir::Value* getImmutStackAllocValue(fir::Value* initValue, std::string name = "");

		std::string printAst(Ast::Expr*);



		std::tuple<FunctionTree*, std::deque<std::string>, std::deque<std::string>, Ast::StructBase*, fir::Type*>
		unwrapStaticDotOperator(Ast::MemberAccess* ma);

		std::pair<std::pair<fir::Type*, Ast::Result_t>, fir::Type*> resolveStaticDotOperator(Ast::MemberAccess* ma, bool actual = true);

		std::pair<Ast::Func*, fir::Function*> resolveMemberFuncCall(Ast::MemberAccess* ma, Ast::ClassDef* str, Ast::FuncCall* fc);

		fir::Function* tryGetMemberFunctionOfClass(Ast::ClassDef* cls, Ast::VarRef* vr, fir::Value* extra);
		fir::Function* tryDisambiguateFunctionVariableUsingType(Ast::VarRef* vr, std::deque<fir::Function*> cands, fir::Value* extra);

		Ast::Result_t getStaticVariable(Ast::Expr* user, Ast::ClassDef* str, std::string name);


		Ast::Result_t getEnumerationCaseValue(Ast::Expr* user, TypePair_t* enr, std::string casename, bool actual = true);
		Ast::Result_t getEnumerationCaseValue(Ast::Expr* lhs, Ast::Expr* rhs, bool actual = true);


		Ast::Result_t doTupleAccess(fir::Value* selfPtr, Ast::Number* num, bool createPtr);

		Ast::Result_t assignValueToAny(fir::Value* lhsPtr, fir::Value* rhs, fir::Value* rhsPtr);
		Ast::Result_t extractValueFromAny(fir::Type* type, fir::Value* ptr);
		Ast::Result_t makeAnyFromValue(fir::Value* value, fir::Value* valuePtr);



		fir::Function* getFunctionFromModuleWithName(Identifier id, Ast::Expr* user);
		fir::Function* getFunctionFromModuleWithNameAndType(Identifier id, fir::FunctionType* ft, Ast::Expr* user);




		Ast::Result_t createLLVariableArray(fir::Value* ptr, fir::Value* length);
		Ast::Result_t indexLLVariableArray(fir::Value* arr, fir::Value* index);
		Ast::Result_t getLLVariableArrayDataPtr(fir::Value* arrPtr);
		Ast::Result_t getLLVariableArrayLength(fir::Value* arrPtr);


		FuncPair_t tryResolveGenericFunctionCall(Ast::FuncCall* fc);
		FuncPair_t tryResolveGenericFunctionCallUsingCandidates(Ast::FuncCall* fc, std::deque<Ast::Func*> cands);
		FuncPair_t tryResolveGenericFunctionFromCandidatesUsingFunctionType(Ast::Expr* user, std::deque<Ast::Func*> candidates,
			fir::FunctionType* ft);

		FuncPair_t instantiateGenericFunctionUsingParameters(Ast::Expr* user, std::map<std::string, fir::Type*> gtm,
			Ast::Func* func, std::deque<fir::Type*> params);


		Ast::ProtocolDef* resolveProtocolName(Ast::Expr* user, std::string pstr);

		std::deque<Ast::ExtensionDef*> getExtensionsForType(Ast::StructBase* cls);
		std::deque<Ast::ExtensionDef*> getExtensionsWithName(std::string name);
		std::deque<Ast::ExtensionDef*> getExtensionsForBuiltinType(fir::Type* type);





		fir::FTContext* getContext();
		fir::Value* getDefaultValue(Ast::Expr* e);
		bool verifyAllPathsReturn(Ast::Func* func, size_t* stmtCounter, bool checkType, fir::Type* retType = 0);

		fir::Type* getExprTypeOfBuiltin(std::string type);
		Ast::ArithmeticOp determineArithmeticOp(std::string ch);
		fir::Instruction getBinaryOperator(Ast::ArithmeticOp op, bool isSigned, bool isFP);
		fir::Function* getStructInitialiser(Ast::Expr* user, TypePair_t* pair, std::vector<fir::Value*> args);
		Ast::Result_t doPointerArithmetic(Ast::ArithmeticOp op, fir::Value* lhs, fir::Value* lhsptr, fir::Value* rhs);
		Ast::Result_t callTypeInitialiser(TypePair_t* tp, Ast::Expr* user, std::vector<fir::Value*> args);

		_OpOverloadData getBinaryOperatorOverload(Ast::Expr* u, Ast::ArithmeticOp op, fir::Type* lhs, fir::Type* rhs);

		Ast::Result_t callBinaryOperatorOverload(_OpOverloadData data, fir::Value* lhs, fir::Value* lhsRef, fir::Value* rhs, fir::Value* rhsRef, Ast::ArithmeticOp op);


		~CodegenInstance();
	};

	void doCodegen(std::string filename, Ast::Root* root, CodegenInstance* cgi);
}



