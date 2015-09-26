// ast.h
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once

#ifndef __STDC_CONSTANT_MACROS
#define __STDC_CONSTANT_MACROS
#endif

#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS
#endif

#include <map>
#include <string>
#include <deque>

#include "typeinfo.h"
#include "defs.h"

namespace llvm
{
	class PHINode;
	class StructType;
}

namespace Ast
{
	enum class ArithmeticOp
	{
		Invalid,
		Add,
		Subtract,
		Multiply,
		Divide,
		Modulo,
		ShiftLeft,
		ShiftRight,
		Assign,

		CmpLT,
		CmpGT,
		CmpLEq,
		CmpGEq,
		CmpEq,
		CmpNEq,

		LogicalNot,
		Plus,
		Minus,

		AddrOf,
		Deref,

		BitwiseAnd,
		BitwiseOr,
		BitwiseXor,
		BitwiseNot,

		LogicalAnd,
		LogicalOr,

		Cast,
		ForcedCast,

		PlusEquals,
		MinusEquals,
		MultiplyEquals,
		DivideEquals,
		ModEquals,
		ShiftLeftEquals,
		ShiftRightEquals,
		BitwiseAndEquals,
		BitwiseOrEquals,
		BitwiseXorEquals,
		MemberAccess,
		ScopeResolution,
		TupleSeparator,
	};

	enum class FFIType
	{
		C,
		Cpp,
	};

	extern uint32_t Attr_Invalid;
	extern uint32_t Attr_NoMangle;
	extern uint32_t Attr_VisPublic;
	extern uint32_t Attr_VisInternal;
	extern uint32_t Attr_VisPrivate;
	extern uint32_t Attr_ForceMangle;
	extern uint32_t Attr_NoAutoInit;
	extern uint32_t Attr_PackedStruct;
	extern uint32_t Attr_StrongTypeAlias;
	extern uint32_t Attr_RawString;
	extern uint32_t Attr_Override;

	typedef std::pair<llvm::Value*, llvm::Value*> ValPtr_t;
	enum class ResultType { Normal, BreakCodegen };
	struct Result_t
	{
		explicit Result_t(ValPtr_t vp) : result(vp), type(ResultType::Normal) { }


		Result_t(llvm::Value* val, llvm::Value* ptr, ResultType rt) : result(val, ptr), type(rt) { }
		Result_t(llvm::Value* val, llvm::Value* ptr) : result(val, ptr), type(ResultType::Normal) { }

		Result_t(ValPtr_t vp, ResultType rt) : result(vp), type(rt) { }

		ValPtr_t result;
		ResultType type;
	};


	// not to be confused with TypeKind
	struct ExprType
	{
		bool isLiteral = true;
		std::string strType;

		Expr* type = 0;

		ExprType() : isLiteral(true), strType(""), type(0) { }
		ExprType(std::string s) : isLiteral(true), strType(s), type(0) { }

		void operator=(std::string stryp)
		{
			this->strType = stryp;
			this->isLiteral = true;
		}
	};

	struct AstDependency
	{
		std::string name;
		Expr* dep;
	};








	struct Expr
	{
		Expr(Parser::PosInfo pos) : posinfo(pos) { }
		virtual ~Expr() { }
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, llvm::Value* lhsPtr = 0, llvm::Value* rhs = 0) = 0;
		virtual bool isBreaking() { return false; }

		bool didCodegen = false;
		uint32_t attribs = 0;
		Parser::PosInfo posinfo;
		std::deque<AstDependency> dependencies;
		ExprType type;
	};

	struct DummyExpr : Expr
	{
		DummyExpr(Parser::PosInfo pos) : Expr(pos) { }
		~DummyExpr();
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, llvm::Value* lhsPtr = 0, llvm::Value* rhs = 0) override { return Result_t(0, 0); }
	};

	struct VarArg : Expr
	{
		~VarArg();
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, llvm::Value* lhsPtr = 0, llvm::Value* rhs = 0) override { return Result_t(0, 0); }
	};


	struct Number : Expr
	{
		~Number();
		Number(Parser::PosInfo pos, double val) : Expr(pos), dval(val) { this->decimal = true; }
		Number(Parser::PosInfo pos, int64_t val) : Expr(pos), ival(val) { this->decimal = false; }
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, llvm::Value* lhsPtr = 0, llvm::Value* rhs = 0) override;

		bool decimal = false;
		llvm::Type* properLlvmType = 0;
		union
		{
			int64_t ival;
			double dval;
		};
	};

	struct BoolVal : Expr
	{
		~BoolVal();
		BoolVal(Parser::PosInfo pos, bool val) : Expr(pos), val(val) { }
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, llvm::Value* lhsPtr = 0, llvm::Value* rhs = 0) override;

		bool val;
	};

	struct VarRef : Expr
	{
		~VarRef();
		VarRef(Parser::PosInfo pos, std::string name) : Expr(pos), name(name) { }
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, llvm::Value* lhsPtr = 0, llvm::Value* rhs = 0) override;

		std::string name;
	};

	struct VarDecl : Expr
	{
		~VarDecl();
		VarDecl(Parser::PosInfo pos, std::string name, bool immut) : Expr(pos), name(name), immutable(immut) { }
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, llvm::Value* lhsPtr = 0, llvm::Value* rhs = 0) override;

		llvm::Value* doInitialValue(Codegen::CodegenInstance* cgi, Codegen::TypePair_t* type, llvm::Value* val, llvm::Value* valptr, llvm::Value* storage, bool shouldAddToSymtab);

		void inferType(Codegen::CodegenInstance* cgi);

		std::string name;
		bool immutable;

		bool isStatic = false;
		bool isGlobal = false;
		bool disableAutoInit = false;
		Expr* initVal = 0;
		llvm::Type* inferredLType = 0;
	};

	struct BracedBlock;
	struct ComputedProperty : VarDecl
	{
		~ComputedProperty();
		ComputedProperty(Parser::PosInfo pos, std::string name) : VarDecl(pos, name, false) { }
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, llvm::Value* lhsPtr = 0, llvm::Value* rhs = 0) override;

		FuncDecl* getterFunc = 0;
		FuncDecl* setterFunc = 0;
		std::string setterArgName;
		BracedBlock* getter = 0;
		BracedBlock* setter = 0;
	};

	struct BinOp : Expr
	{
		~BinOp();
		BinOp(Parser::PosInfo pos, Expr* lhs, ArithmeticOp operation, Expr* rhs) : Expr(pos), left(lhs), right(rhs), op(operation) { }
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, llvm::Value* lhsPtr = 0, llvm::Value* rhs = 0) override;

		Expr* left;
		Expr* right;

		ArithmeticOp op;
		llvm::PHINode* phi = 0;
	};

	struct StructBase;
	struct FuncDecl : Expr
	{
		~FuncDecl();
		FuncDecl(Parser::PosInfo pos, std::string id, std::deque<VarDecl*> params, std::string ret) : Expr(pos), name(id), params(params)
		{ this->type.strType = ret; }
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, llvm::Value* lhsPtr = 0, llvm::Value* rhs = 0) override;

		Result_t generateDeclForGenericType(Codegen::CodegenInstance* cgi, std::map<std::string, llvm::Type*> types);

		bool hasVarArg = false;
		bool isFFI = false;
		bool isStatic = false;
		bool wasCalled = false;

		StructBase* parentClass = nullptr;
		FFIType ffiType = FFIType::C;
		std::string name;
		std::string mangledName;
		std::string mangledNamespaceOnly;

		std::deque<VarDecl*> params;
		std::deque<std::string> genericTypes;

		llvm::Type* instantiatedGenericReturnType = 0;
		std::deque<llvm::Type*> instantiatedGenericTypes;
	};

	struct DeferredExpr;
	struct BracedBlock : Expr
	{
		BracedBlock(Parser::PosInfo pos) : Expr(pos) { }
		~BracedBlock();
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, llvm::Value* lhsPtr = 0, llvm::Value* rhs = 0) override;

		std::deque<Expr*> statements;
		std::deque<DeferredExpr*> deferredStatements;
	};

	struct Func : Expr
	{
		~Func();
		Func(Parser::PosInfo pos, FuncDecl* funcdecl, BracedBlock* block) : Expr(pos), decl(funcdecl), block(block) { }
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, llvm::Value* lhsPtr = 0, llvm::Value* rhs = 0) override;

		FuncDecl* decl;
		BracedBlock* block;

		std::deque<std::deque<llvm::Type*>> instantiatedGenericVersions;
	};

	struct FuncCall : Expr
	{
		~FuncCall();
		FuncCall(Parser::PosInfo pos, std::string target, std::deque<Expr*> args) : Expr(pos), name(target), params(args) { }
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, llvm::Value* lhsPtr = 0, llvm::Value* rhs = 0) override;

		std::string name;
		std::deque<Expr*> params;

		llvm::Function* cachedGenericFuncTarget = 0;
		Codegen::Resolved_t cachedResolveTarget;
	};

	struct Return : Expr
	{
		~Return();
		Return(Parser::PosInfo pos, Expr* e) : Expr(pos), val(e) { }
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, llvm::Value* lhsPtr = 0, llvm::Value* rhs = 0) override;
		virtual bool isBreaking() override { return true; }

		Expr* val;
		llvm::Value* actualReturnValue = 0;
	};

	struct Import : Expr
	{
		~Import();
		Import(Parser::PosInfo pos, std::string name) : Expr(pos), module(name) { }
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, llvm::Value* lhsPtr = 0, llvm::Value* rhs = 0) override { return Result_t(nullptr, nullptr); }

		std::string module;
	};

	struct ForeignFuncDecl : Expr
	{
		~ForeignFuncDecl();
		ForeignFuncDecl(Parser::PosInfo pos, FuncDecl* func) : Expr(pos), decl(func) { }
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, llvm::Value* lhsPtr = 0, llvm::Value* rhs = 0) override;

		FuncDecl* decl;
	};

	struct DeferredExpr : Expr
	{
		DeferredExpr(Parser::PosInfo pos, Expr* e) : Expr(pos), expr(e) { }
		~DeferredExpr();

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, llvm::Value* lhsPtr = 0, llvm::Value* rhs = 0) override;

		Expr* expr;
	};

	struct BreakableBracedBlock : Expr
	{
		BreakableBracedBlock(Parser::PosInfo pos) : Expr(pos) { }
		~BreakableBracedBlock();
	};

	struct If : Expr
	{
		~If();
		If(Parser::PosInfo pos, std::deque<std::pair<Expr*, BracedBlock*>> cases, BracedBlock* ecase) : Expr(pos),
			final(ecase), cases(cases), _cases(cases) { }

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, llvm::Value* lhsPtr = 0, llvm::Value* rhs = 0) override;


		BracedBlock* final;
		std::deque<std::pair<Expr*, BracedBlock*>> cases;
		std::deque<std::pair<Expr*, BracedBlock*>> _cases;	// needed to preserve stuff, since If->codegen modifies this->cases
	};

	struct WhileLoop : BreakableBracedBlock
	{
		~WhileLoop();
		WhileLoop(Parser::PosInfo pos, Expr* _cond, BracedBlock* _body, bool dowhile) : BreakableBracedBlock(pos),
			cond(_cond), body(_body), isDoWhileVariant(dowhile) { }

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, llvm::Value* lhsPtr = 0, llvm::Value* rhs = 0) override;

		Expr* cond;
		BracedBlock* body;
		bool isDoWhileVariant;
	};

	struct ForLoop : BreakableBracedBlock
	{
		~ForLoop();
		ForLoop(Parser::PosInfo pos, VarDecl* _var, Expr* _cond, Expr* _eval) : BreakableBracedBlock(pos),
			var(_var), cond(_cond), eval(_eval) { }

		VarDecl* var;
		Expr* cond;
		Expr* eval;
	};

	struct ForeachLoop : BreakableBracedBlock
	{

	};

	struct Break : Expr
	{
		~Break();
		Break(Parser::PosInfo pos) : Expr(pos) { }
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, llvm::Value* lhsPtr = 0, llvm::Value* rhs = 0) override;
		virtual bool isBreaking() override { return true; }
	};

	struct Continue : Expr
	{
		~Continue();
		Continue(Parser::PosInfo pos) : Expr(pos) { }
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, llvm::Value* lhsPtr = 0, llvm::Value* rhs = 0) override;
		virtual bool isBreaking() override { return true; }
	};

	struct UnaryOp : Expr
	{
		~UnaryOp();
		UnaryOp(Parser::PosInfo pos, ArithmeticOp op, Expr* expr) : Expr(pos), op(op), expr(expr) { }
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, llvm::Value* lhsPtr = 0, llvm::Value* rhs = 0) override;

		ArithmeticOp op;
		Expr* expr;
	};

	// fuck
	struct StructBase;
	struct OpOverload : Expr
	{
		~OpOverload();
		OpOverload(Parser::PosInfo pos, ArithmeticOp op) : Expr(pos), op(op) { }
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, llvm::Value* lhsPtr = 0, llvm::Value* rhs = 0) override;

		Func* func = 0;
		ArithmeticOp op;
		StructBase* str = 0;
	};

	struct StructBase : Expr
	{
		virtual ~StructBase();
		StructBase(Parser::PosInfo pos, std::string name) : Expr(pos), name(name) { }
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, llvm::Value* lhsPtr = 0, llvm::Value* rhs = 0) override = 0;
		virtual llvm::Type* createType(Codegen::CodegenInstance* cgi) = 0;

		bool didCreateType = false;

		std::string name;
		std::string mangledName;

		std::deque<VarDecl*> members;
		std::deque<std::string> scope;
		std::map<std::string, int> nameMap;
		std::deque<OpOverload*> opOverloads;
		std::deque<std::pair<ArithmeticOp, llvm::Function*>> lOpOverloads;
	};

	struct Extension;
	struct Class : StructBase
	{
		~Class();
		Class(Parser::PosInfo pos, std::string name) : StructBase(pos, name) { }
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, llvm::Value* lhsPtr = 0, llvm::Value* rhs = 0) override;
		virtual llvm::Type* createType(Codegen::CodegenInstance* cgi) override;

		std::deque<Func*> funcs;
		std::deque<Extension*> extensions;
		std::deque<llvm::Function*> lfuncs;
		std::deque<ComputedProperty*> cprops;
		std::deque<std::string> protocolstrs;
		std::deque<llvm::Function*> initFuncs;
		std::pair<Class*, llvm::StructType*> superclass;
		std::deque<std::pair<Class*, llvm::Type*>> nestedTypes;
	};

	// extends class, because it's basically a class, except we need to apply it to an existing class
	struct Extension : Class
	{
		~Extension();
		Extension(Parser::PosInfo pos, std::string name) : Class(pos, name) { }
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, llvm::Value* lhsPtr = 0, llvm::Value* rhs = 0) override;
		virtual llvm::Type* createType(Codegen::CodegenInstance* cgi) override;

		llvm::Function* createAutomaticInitialiser(Codegen::CodegenInstance* cgi, llvm::StructType* stype, int extIndex);
	};




	struct Struct : StructBase
	{
		~Struct();
		Struct(Parser::PosInfo pos, std::string name) : StructBase(pos, name) { }
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, llvm::Value* lhsPtr = 0, llvm::Value* rhs = 0) override;
		virtual llvm::Type* createType(Codegen::CodegenInstance* cgi) override;

		bool packed = false;
		llvm::Function* initFunc;
		std::deque<Struct*> imports;
	};

	struct Enumeration : Class
	{
		~Enumeration();
		Enumeration(Parser::PosInfo pos, std::string name) : Class(pos, name) { }
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, llvm::Value* lhsPtr = 0, llvm::Value* rhs = 0) override;
		virtual llvm::Type* createType(Codegen::CodegenInstance* cgi) override;

		std::deque<std::pair<std::string, Expr*>> cases;
		bool isStrong = false;
	};

	struct Tuple : StructBase
	{
		~Tuple();
		Tuple(Parser::PosInfo pos, std::vector<Expr*> _values) : StructBase(pos, ""), values(_values) { }
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, llvm::Value* lhsPtr = 0, llvm::Value* rhs = 0) override;
		virtual llvm::Type* createType(Codegen::CodegenInstance* cgi) override;
		llvm::StructType* getType(Codegen::CodegenInstance* cgi);

		std::vector<Expr*> values;
		std::vector<llvm::Type*> ltypes;

		llvm::StructType* cachedLlvmType = 0;
	};


	enum class MAType
	{
		Invalid,
		LeftNamespace,
		LeftVariable,
		LeftFunctionCall,
		LeftTypename
	};

	struct MemberAccess : Expr
	{
		~MemberAccess();
		MemberAccess(Parser::PosInfo pos, Expr* _left, Expr* _right) : Expr(pos), left(_left), right(_right) { }
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, llvm::Value* lhsPtr = 0, llvm::Value* rhs = 0) override;

		bool disableStaticChecking = false;
		Result_t cachedCodegenResult = Result_t(0, 0);
		Expr* left;
		Expr* right;

		MAType matype = MAType::Invalid;
	};




	struct NamespaceDecl : Expr
	{
		~NamespaceDecl();
		NamespaceDecl(Parser::PosInfo pos, std::string _name, BracedBlock* inside) : Expr(pos), innards(inside), name(_name)
		{ }
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, llvm::Value* lhsPtr = 0, llvm::Value* rhs = 0) override { return Result_t(0, 0); }

		void codegenPass(Codegen::CodegenInstance* cgi, int pass);

		std::deque<NamespaceDecl*> namespaces;
		BracedBlock* innards;
		std::string name;
	};

	struct ArrayIndex : Expr
	{
		~ArrayIndex();
		ArrayIndex(Parser::PosInfo pos, Expr* v, Expr* index) : Expr(pos), arr(v), index(index) { }
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, llvm::Value* lhsPtr = 0, llvm::Value* rhs = 0) override;

		Expr* arr;
		Expr* index;
	};

	struct StringLiteral : Expr
	{
		~StringLiteral();
		StringLiteral(Parser::PosInfo pos, std::string str) : Expr(pos), str(str) { }
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, llvm::Value* lhsPtr = 0, llvm::Value* rhs = 0) override;

		bool isRaw = false;
		std::string str;
	};

	struct ArrayLiteral : Expr
	{
		~ArrayLiteral();
		ArrayLiteral(Parser::PosInfo pos, std::deque<Expr*> values) : Expr(pos), values(values) { }
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, llvm::Value* lhsPtr = 0, llvm::Value* rhs = 0) override;

		std::deque<Expr*> values;
	};

	struct TypeAlias : StructBase
	{
		~TypeAlias();
		TypeAlias(Parser::PosInfo pos, std::string _alias, std::string _origType) : StructBase(pos, _alias), origType(_origType) { }
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, llvm::Value* lhsPtr = 0, llvm::Value* rhs = 0) override;
		virtual llvm::Type* createType(Codegen::CodegenInstance* cgi) override;

		bool isStrong = false;
		std::string origType;
	};

	struct Alloc : Expr
	{
		~Alloc();
		Alloc(Parser::PosInfo pos) : Expr(pos) { }
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, llvm::Value* lhsPtr = 0, llvm::Value* rhs = 0) override;

		Expr* count = 0;
		std::deque<Expr*> params;
	};

	struct Dealloc : Expr
	{
		~Dealloc();
		Dealloc(Parser::PosInfo pos, Expr* _expr) : Expr(pos), expr(_expr) { }
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, llvm::Value* lhsPtr = 0, llvm::Value* rhs = 0) override;

		Expr* expr;
	};

	struct Typeof : Expr
	{
		~Typeof();
		Typeof(Parser::PosInfo pos, Expr* _inside) : Expr(pos), inside(_inside) { }
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, llvm::Value* lhsPtr = 0, llvm::Value* rhs = 0) override;

		Expr* inside;
	};

	struct PostfixUnaryOp : Expr
	{
		enum class Kind
		{
			Invalid,
			ArrayIndex,
			Increment,
			Decrement
		};

		~PostfixUnaryOp();
		PostfixUnaryOp(Parser::PosInfo pos, Expr* e, Kind k) : Expr(pos), kind(k), expr(e) { }
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, llvm::Value* lhsPtr = 0, llvm::Value* rhs = 0) override;

		Kind kind;
		Expr* expr;
		std::deque<Expr*> args;
	};

	struct Root : Expr
	{
		Root() : Expr(Parser::PosInfo()), publicFuncTree("__#root"), externalFuncTree("__#root") { }
		~Root();
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, llvm::Value* lhsPtr = 0, llvm::Value* rhs = 0) override;

		// public functiondecls and type decls.
		Codegen::FunctionTree publicFuncTree;
		std::deque<std::pair<StructBase*, llvm::Type*>> publicTypes;

		// list of all function calls. all.
		std::deque<FuncCall*> allFunctionCalls;

		// list of all functions. every single one.
		std::deque<Func*> allFunctionBodies;

		// list of all generic functions that we know about, as well as import + export.
		std::deque<FuncDecl*> genericFunctions;
		std::deque<std::pair<FuncDecl*, Func*>> externalGenericFunctions;
		std::deque<std::pair<FuncDecl*, Func*>> publicGenericFunctions;

		// imported types. these exist, but we need to declare them manually while code-generating.
		Codegen::FunctionTree externalFuncTree;
		std::deque<std::pair<StructBase*, llvm::Type*>> externalTypes;

		// libraries referenced by 'import'
		std::deque<std::string> referencedLibraries;

		// top level stuff
		std::deque<Expr*> topLevelExpressions;
		std::deque<NamespaceDecl*> topLevelNamespaces;

		std::vector<std::tuple<std::string, llvm::Type*, Codegen::TypeKind>> typeList;

		// the module-level global constructor trampoline that initialises static and global variables
		// that require init().
		// this will be called by a top-level trampoline that calls everything when all the modules are linked together
		llvm::Function* globalConstructorTrampoline = 0;
	};
}









