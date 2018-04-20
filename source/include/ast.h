// ast.h
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once
#include "defs.h"
#include "sst_expr.h"
#include "stcommon.h"
#include "precompile.h"

#include <unordered_set>

namespace pts
{
	struct Type;
}

namespace fir
{
	struct Type;
}

namespace sst
{
	struct TypeDefn;
	struct TypecheckState;
	struct FunctionDefn;
	struct FunctionDecl;
}

namespace ast
{
	struct Stmt : Locatable
	{
		Stmt(const Location& l) : Locatable(l, "statement") { }
		virtual ~Stmt();
		virtual TCResult typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) = 0;
	};

	struct Expr : Stmt
	{
		Expr(const Location& l) : Stmt(l) { this->readableName = "expression"; }
		~Expr();

		virtual TCResult typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) = 0;
	};

	struct DeferredStmt : Stmt
	{
		DeferredStmt(const Location& l) : Stmt(l) { this->readableName = "deferred statement"; }
		~DeferredStmt() { }

		virtual TCResult typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;

		Stmt* actual = 0;
	};

	struct Parameterisable : Stmt
	{
		Parameterisable(const Location& l) : Stmt(l) { this->readableName = "<Parameterisable>"; }
		~Parameterisable() { }

		virtual TCResult generateDeclaration(sst::TypecheckState* fs, fir::Type* infer, const TypeParamMap_t& gmaps) = 0;
		virtual TCResult typecheck(sst::TypecheckState* fs, fir::Type* infer, const TypeParamMap_t& gmaps) = 0;

		//* anything with generic abilities must implement the version of generateDeclaration and typecheck that accommodates the mapping argument
		//* if not we won't be able to know anything about anything.

		//? typecheck method is implemented for Parameterisable (and marked final) in typecheck/misc.cpp, where it simply calls the generic typecheck
		//? with an empty mapping. It is up to the individual AST during typechecking to verify `!gmaps.empty()` if `this->generics.size() > 0`.
		virtual TCResult typecheck(sst::TypecheckState* fs, fir::Type* infer = 0) final override;

		std::pair<bool, sst::Defn*> checkForExistingDeclaration(sst::TypecheckState* fs, const TypeParamMap_t& gmaps);

		std::string name;

		std::unordered_map<std::string, TypeConstraints_t> generics;
		std::vector<std::pair<sst::Defn*, TypeParamMap_t>> genericVersions;

		// kind of a hack.
		std::unordered_set<sst::Defn*> finishedTypechecking;
	};


	struct ImportStmt : Stmt
	{
		ImportStmt(const Location& l, std::string p) : Stmt(l), path(p) { this->readableName = "import statement"; }
		~ImportStmt() { }

		virtual TCResult typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;

		std::string path;
		std::string resolvedModule;

		std::string importAs;
	};

	struct Block : Stmt
	{
		Block(const Location& l) : Stmt(l) { this->readableName = "block"; }
		~Block() { }

		virtual TCResult typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;

		Location closingBrace;

		bool isArrow = false;
		std::vector<Stmt*> statements;
		std::vector<Stmt*> deferredStatements;
	};




	struct FuncDefn : Parameterisable
	{
		FuncDefn(const Location& l) : Parameterisable(l) { this->readableName = "function defintion"; }
		~FuncDefn() { }

		virtual TCResult typecheck(sst::TypecheckState* fs, fir::Type* infer, const TypeParamMap_t& gmaps) override;
		virtual TCResult generateDeclaration(sst::TypecheckState* fs, fir::Type* infer, const TypeParamMap_t& gmaps) override;

		struct Arg
		{
			std::string name;
			Location loc;
			pts::Type* type = 0;
		};


		std::vector<Arg> args;
		pts::Type* returnType = 0;

		Block* body = 0;

		VisibilityLevel visibility = VisibilityLevel::Internal;

		bool isEntry = false;
		bool noMangle = false;

		bool isMutating = false;

		bool isVirtual = false;
		bool isOverride = false;
	};

	struct InitFunctionDefn : Parameterisable
	{
		InitFunctionDefn(const Location& l) : Parameterisable(l) { this->readableName = "class initialiser definition"; }
		~InitFunctionDefn() { }

		virtual TCResult typecheck(sst::TypecheckState* fs, fir::Type* infer, const TypeParamMap_t& gmaps) override;
		virtual TCResult generateDeclaration(sst::TypecheckState* fs, fir::Type* infer, const TypeParamMap_t& gmaps) override;

		using Arg = FuncDefn::Arg;

		std::vector<Arg> args;

		bool didCallSuper = false;
		std::vector<std::pair<std::string, Expr*>> superArgs;

		Block* body = 0;

		FuncDefn* actualDefn = 0;
	};

	struct ForeignFuncDefn : Stmt
	{
		ForeignFuncDefn(const Location& l) : Stmt(l) { this->readableName = "foreign function definition"; }
		~ForeignFuncDefn() { }

		sst::FunctionDecl* generatedDecl = 0;
		virtual TCResult typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;

		using Arg = FuncDefn::Arg;

		std::string name;

		std::vector<Arg> args;
		pts::Type* returnType = 0;

		bool isVarArg = false;
		VisibilityLevel visibility = VisibilityLevel::Internal;
	};

	struct OperatorOverloadDefn : FuncDefn
	{
		OperatorOverloadDefn(const Location& l) : FuncDefn(l) { this->readableName = "operator overload defintion"; }
		~OperatorOverloadDefn() { }

		virtual TCResult typecheck(sst::TypecheckState* fs, fir::Type* infer, const TypeParamMap_t& gmaps) override;
		virtual TCResult generateDeclaration(sst::TypecheckState* fs, fir::Type* infer, const TypeParamMap_t& gmaps) override;

		enum class Kind
		{
			Invalid,
			Infix,
			Prefix,
			Postfix
		};

		std::string symbol;
		Kind kind = Kind::Invalid;
	};

	struct VarDefn : Stmt
	{
		VarDefn(const Location& l) : Stmt(l) { this->readableName = "variable defintion"; }
		~VarDefn() { }

		virtual TCResult typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;

		std::string name;
		pts::Type* type = 0;

		bool immut = false;
		Expr* initialiser = 0;

		VisibilityLevel visibility = VisibilityLevel::Internal;
		bool noMangle = false;
	};


	struct DecompVarDefn : Stmt
	{
		DecompVarDefn(const Location& l) : Stmt(l) { this->readableName = "destructuring variable defintion"; }
		~DecompVarDefn() { }

		virtual TCResult typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;

		bool immut = false;
		Expr* initialiser = 0;
		DecompMapping bindings;
	};

	struct IfStmt : Stmt
	{
		IfStmt(const Location& l) : Stmt(l) { this->readableName = "if statement"; }
		~IfStmt() { }

		virtual TCResult typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;

		struct Case
		{
			Expr* cond = 0;
			Block* body = 0;

			std::vector<Stmt*> inits;
		};

		std::vector<Case> cases;
		Block* elseCase = 0;
	};

	struct ReturnStmt : Stmt
	{
		ReturnStmt(const Location& l) : Stmt(l) { this->readableName = "return statement"; }
		~ReturnStmt() { }

		virtual TCResult typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;

		Expr* value = 0;
	};

	struct WhileLoop : Stmt
	{
		WhileLoop(const Location& l) : Stmt(l) { this->readableName = "while loop"; }
		~WhileLoop() { }

		virtual TCResult typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;

		Expr* cond = 0;
		Block* body = 0;

		bool isDoVariant = false;
	};

	struct ForLoop : Stmt
	{
		ForLoop(const Location& l) : Stmt(l) { this->readableName = "for loop"; }
		~ForLoop() { }

		virtual TCResult typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override = 0;

		Block* body = 0;
	};

	struct ForeachLoop : ForLoop
	{
		ForeachLoop(const Location& l) : ForLoop(l) { this->readableName = "for loop"; }
		~ForeachLoop() { }

		virtual TCResult typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;

		Expr* array = 0;

		std::string indexVar;
		DecompMapping bindings;
	};


	struct BreakStmt : Stmt
	{
		BreakStmt(const Location& l) : Stmt(l) { this->readableName = "break statement"; }
		~BreakStmt() { }

		virtual TCResult typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;
	};

	struct ContinueStmt : Stmt
	{
		ContinueStmt(const Location& l) : Stmt(l) { this->readableName = "continue statement"; }
		~ContinueStmt() { }

		virtual TCResult typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;
	};

	struct UsingStmt : Stmt
	{
		UsingStmt(const Location& l) : Stmt(l) { this->readableName = "using statement"; }
		~UsingStmt() { }

		virtual TCResult typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;

		Expr* expr = 0;
		std::string useAs;
	};

	struct StaticDecl : Stmt
	{
		StaticDecl(Stmt* s) : Stmt(s->loc), actual(s) { this->readableName = "static declaration"; }
		~StaticDecl() { }

		virtual TCResult typecheck(sst::TypecheckState* fs, fir::Type* inf = 0) override { return this->actual->typecheck(fs, inf); }

		Stmt* actual = 0;
	};


	struct VirtualDecl : Stmt
	{
		VirtualDecl(Stmt* s) : Stmt(s->loc), actual(s) { this->readableName = "virtual declaration"; }
		~VirtualDecl() { }

		virtual TCResult typecheck(sst::TypecheckState* fs, fir::Type* inf = 0) override { return this->actual->typecheck(fs, inf); }

		Stmt* actual = 0;
		bool isOverride = false;
	};


	struct TypeDefn : Parameterisable
	{
		TypeDefn(const Location& l) : Parameterisable(l) { this->readableName = "type definition"; }
		~TypeDefn() { }

		VisibilityLevel visibility = VisibilityLevel::Internal;
	};

	struct StructDefn : TypeDefn
	{
		StructDefn(const Location& l) : TypeDefn(l) { this->readableName = "struct definition"; }
		~StructDefn() { }

		virtual TCResult typecheck(sst::TypecheckState* fs, fir::Type* infer, const TypeParamMap_t& gmaps) override;
		virtual TCResult generateDeclaration(sst::TypecheckState* fs, fir::Type* infer, const TypeParamMap_t& gmaps) override;

		std::vector<pts::Type*> bases;

		std::vector<VarDefn*> fields;
		std::vector<FuncDefn*> methods;

		std::vector<VarDefn*> staticFields;
		std::vector<FuncDefn*> staticMethods;

		std::vector<TypeDefn*> nestedTypes;
	};

	struct ClassDefn : TypeDefn
	{
		ClassDefn(const Location& l) : TypeDefn(l) { this->readableName = "class definition"; }
		~ClassDefn() { }

		virtual TCResult typecheck(sst::TypecheckState* fs, fir::Type* infer, const TypeParamMap_t& gmaps) override;
		virtual TCResult generateDeclaration(sst::TypecheckState* fs, fir::Type* infer, const TypeParamMap_t& gmaps) override;

		std::vector<pts::Type*> bases;

		std::vector<InitFunctionDefn*> initialisers;

		std::vector<VarDefn*> fields;
		std::vector<FuncDefn*> methods;

		std::vector<VarDefn*> staticFields;
		std::vector<FuncDefn*> staticMethods;

		std::vector<TypeDefn*> nestedTypes;
	};

	struct EnumDefn : TypeDefn
	{
		EnumDefn(const Location& l) : TypeDefn(l) { this->readableName = "enum definition"; }
		~EnumDefn() { }

		virtual TCResult typecheck(sst::TypecheckState* fs, fir::Type* infer, const TypeParamMap_t& gmaps) override;
		virtual TCResult generateDeclaration(sst::TypecheckState* fs, fir::Type* infer, const TypeParamMap_t& gmaps) override;

		struct Case
		{
			Location loc;
			std::string name;
			Expr* value = 0;
		};

		std::vector<Case> cases;
		pts::Type* memberType = 0;
	};

	struct TypeExpr : Expr
	{
		TypeExpr(const Location& l, pts::Type* t) : Expr(l), type(t) { this->readableName = "<TYPE EXPRESSION>"; }
		~TypeExpr() { }

		virtual TCResult typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;

		pts::Type* type = 0;
	};

	// a bit of a strange thing, but basically it's a kind of cast.
	struct MutabilityTypeExpr : Expr
	{
		MutabilityTypeExpr(const Location& l, bool m) : Expr(l), mut(m) { this->readableName = "<TYPE EXPRESSION>"; }
		~MutabilityTypeExpr() { }

		virtual TCResult typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;

		bool mut;
	};

	struct Ident : Expr
	{
		Ident(const Location& l, std::string n) : Expr(l), name(n) { this->readableName = "identifier"; }
		~Ident() { }

		virtual TCResult typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;

		std::string name;
		bool traverseUpwards = true;

		// for these cases: Foo<T: int>(...) and Foo<T: int>.staticAccess
		// where Foo is, respectively, a generic function and a generic type.
		std::unordered_map<std::string, pts::Type*> mappings;
	};


	struct RangeExpr : Expr
	{
		RangeExpr(const Location& loc) : Expr(loc) { this->readableName = "range expression"; }
		~RangeExpr() { }

		virtual TCResult typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;

		Expr* start = 0;
		Expr* end = 0;

		Expr* step = 0;

		bool halfOpen = false;
	};



	struct AllocOp : Expr
	{
		AllocOp(const Location& l) : Expr(l) { this->readableName = "alloc statement"; }
		~AllocOp() { }

		virtual TCResult typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;

		pts::Type* allocTy = 0;
		std::vector<Expr*> counts;
		std::vector<std::pair<std::string, Expr*>> args;

		Block* initBody = 0;

		bool isRaw = false;
		bool isMutable = false;
	};

	struct DeallocOp : Stmt
	{
		DeallocOp(const Location& l) : Stmt(l) { this->readableName = "free statement"; }
		~DeallocOp() { }

		virtual TCResult typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;
		Expr* expr = 0;
	};

	struct SizeofOp : Expr
	{
		SizeofOp(const Location& l) : Expr(l) { this->readableName = "sizeof expression"; }
		~SizeofOp() { }

		virtual TCResult typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;
		Expr* expr = 0;
	};


	struct BinaryOp : Expr
	{
		BinaryOp(const Location& loc, std::string o, Expr* l, Expr* r) : Expr(loc), op(o), left(l), right(r) { this->readableName = "binary expression"; }
		~BinaryOp() { }

		virtual TCResult typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;

		std::string op;

		Expr* left = 0;
		Expr* right = 0;
	};

	struct UnaryOp : Expr
	{
		UnaryOp(const Location& l) : Expr(l) { this->readableName = "unary expression"; }
		~UnaryOp() { }

		virtual TCResult typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;

		std::string op;
		Expr* expr = 0;
		bool isPostfix = false;
	};

	struct AssignOp : Expr
	{
		AssignOp(const Location& l) : Expr(l) { this->readableName = "assignment statement"; }
		~AssignOp() { }

		virtual TCResult typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;

		std::string op;

		Expr* left = 0;
		Expr* right = 0;
	};

	struct SubscriptOp : Expr
	{
		SubscriptOp(const Location& l) : Expr(l) { }
		~SubscriptOp() { }

		virtual TCResult typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;

		Expr* expr = 0;
		Expr* inside = 0;
	};

	struct SliceOp : Expr
	{
		SliceOp(const Location& l) : Expr(l) { this->readableName = "slice expression"; }
		~SliceOp() { }

		virtual TCResult typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;

		Expr* expr = 0;

		Expr* start = 0;
		Expr* end = 0;
	};

	struct SplatOp : Expr
	{
		SplatOp(const Location& l) : Expr(l) { this->readableName = "splat expression"; }
		~SplatOp() { }

		virtual TCResult typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;

		Expr* expr = 0;
	};





	struct FunctionCall : Expr
	{
		FunctionCall(const Location& l, std::string n) : Expr(l), name(n) { this->readableName = "function call"; }
		~FunctionCall() { }

		virtual TCResult typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;
		sst::Expr* typecheckWithArguments(sst::TypecheckState* fs, const std::vector<FnCallArgument>& args);

		std::string name;
		std::vector<std::pair<std::string, Expr*>> args;

		bool traverseUpwards = true;
	};

	struct ExprCall : Expr
	{
		ExprCall(const Location& l) : Expr(l) { this->readableName = "function call"; }
		~ExprCall() { }

		virtual TCResult typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;
		sst::Expr* typecheckWithArguments(sst::TypecheckState* fs, const std::vector<FnCallArgument>& args);

		Expr* callee = 0;
		std::vector<std::pair<std::string, Expr*>> args;
	};



	struct DotOperator : Expr
	{
		DotOperator(const Location& loc, Expr* l, Expr* r) : Expr(loc), left(l), right(r) { this->readableName = "dot operator"; }
		~DotOperator() { }

		virtual TCResult typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;

		Expr* left = 0;
		Expr* right = 0;
	};




	struct LitNumber : Expr
	{
		LitNumber(const Location& l, std::string n) : Expr(l), num(n) { this->readableName = "number literal"; }
		~LitNumber() { }

		virtual TCResult typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;

		std::string num;
	};

	struct LitBool : Expr
	{
		LitBool(const Location& l, bool val) : Expr(l), value(val) { this->readableName = "boolean literal"; }
		~LitBool() { }

		virtual TCResult typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;

		bool value = false;
	};

	struct LitString : Expr
	{
		LitString(const Location& l, std::string s, bool isc) : Expr(l), str(s), isCString(isc) { this->readableName = "string literal"; }
		~LitString() { }

		virtual TCResult typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;

		std::string str;
		bool isCString = false;
	};

	struct LitNull : Expr
	{
		LitNull(const Location& l) : Expr(l) { this->readableName = "null literal"; }
		~LitNull() { }

		virtual TCResult typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;
	};

	struct LitTuple : Expr
	{
		LitTuple(const Location& l, std::vector<Expr*> its) : Expr(l), values(its) { this->readableName = "tuple literal"; }
		~LitTuple() { }

		virtual TCResult typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;

		std::vector<Expr*> values;
	};

	struct LitArray : Expr
	{
		LitArray(const Location& l, std::vector<Expr*> its) : Expr(l), values(its) { this->readableName = "array literal"; }
		~LitArray() { }

		virtual TCResult typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;

		bool raw = false;
		std::vector<Expr*> values;
	};


	struct TopLevelBlock : Stmt
	{
		TopLevelBlock(const Location& l, std::string n) : Stmt(l), name(n) { this->readableName = "namespace"; }
		~TopLevelBlock() { }

		virtual TCResult typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;

		std::string name;
		std::vector<Stmt*> statements;
		VisibilityLevel visibility = VisibilityLevel::Internal;
	};
}






