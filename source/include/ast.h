// ast.h
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once
#include "defs.h"
#include "sst_expr.h"
#include "stcommon.h"
#include "precompile.h"

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
		virtual sst::Stmt* typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) = 0;
	};

	struct Expr : Stmt
	{
		Expr(const Location& l) : Stmt(l) { this->readableName = "expression"; }
		~Expr();

		virtual sst::Expr* typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) = 0;
	};

	struct DeferredStmt : Stmt
	{
		DeferredStmt(const Location& l) : Stmt(l) { this->readableName = "deferred statement"; }
		~DeferredStmt() { }

		virtual sst::Stmt* typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;

		Stmt* actual = 0;
	};




	struct ImportStmt : Stmt
	{
		ImportStmt(const Location& l, std::string p) : Stmt(l), path(p) { this->readableName = "import statement"; }
		~ImportStmt() { }

		virtual sst::Stmt* typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;

		std::string path;
		std::string resolvedModule;

		std::string importAs;
	};

	struct Block : Stmt
	{
		Block(const Location& l) : Stmt(l) { this->readableName = "block"; }
		~Block() { }

		virtual sst::Stmt* typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;

		Location closingBrace;

		bool isArrow = false;
		std::vector<Stmt*> statements;
		std::vector<Stmt*> deferredStatements;
	};




	struct FuncDefn : Stmt
	{
		FuncDefn(const Location& l) : Stmt(l) { this->readableName = "function defintion"; }
		~FuncDefn() { }

		virtual sst::Stmt* typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;

		sst::FunctionDefn* generatedDefn = 0;
		void generateDeclaration(sst::TypecheckState* fs, fir::Type* infer);

		struct Arg
		{
			std::string name;
			Location loc;
			pts::Type* type = 0;
		};

		std::string name;
		std::map<std::string, TypeConstraints_t> generics;

		std::vector<Arg> args;
		pts::Type* returnType = 0;

		Block* body = 0;

		VisibilityLevel visibility = VisibilityLevel::Internal;

		bool isEntry = false;
		bool noMangle = false;
	};

	struct ForeignFuncDefn : Stmt
	{
		ForeignFuncDefn(const Location& l) : Stmt(l) { this->readableName = "foreign function definition"; }
		~ForeignFuncDefn() { }

		sst::FunctionDecl* generatedDecl = 0;
		virtual sst::Stmt* typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;

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

		void generateDeclaration(sst::TypecheckState* fs, fir::Type* infer);
		virtual sst::Stmt* typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;

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

		virtual sst::Stmt* typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;

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

		virtual sst::Stmt* typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;

		bool immut = false;
		Expr* initialiser = 0;
		DecompMapping bindings;
	};

	struct IfStmt : Stmt
	{
		IfStmt(const Location& l) : Stmt(l) { this->readableName = "if statement"; }
		~IfStmt() { }

		virtual sst::Stmt* typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;

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

		virtual sst::Stmt* typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;

		Expr* value = 0;
	};

	struct WhileLoop : Stmt
	{
		WhileLoop(const Location& l) : Stmt(l) { this->readableName = "while loop"; }
		~WhileLoop() { }

		virtual sst::Stmt* typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;

		Expr* cond = 0;
		Block* body = 0;

		bool isDoVariant = false;
	};

	struct ForLoop : Stmt
	{
		ForLoop(const Location& l) : Stmt(l) { this->readableName = "for loop"; }
		~ForLoop() { }

		virtual sst::Stmt* typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override = 0;

		Block* body = 0;
	};

	struct ForeachLoop : ForLoop
	{
		ForeachLoop(const Location& l) : ForLoop(l) { this->readableName = "for loop"; }
		~ForeachLoop() { }

		virtual sst::Stmt* typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;

		Location varloc;

		std::string var;
		Expr* array = 0;
	};

	struct ForTupleDecompLoop : ForLoop
	{
		ForTupleDecompLoop(const Location& l) : ForLoop(l) { this->readableName = "for loop"; }
		~ForTupleDecompLoop() { }

		virtual sst::Stmt* typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;

		Expr* array = 0;
		DecompMapping bindings;
	};


	struct BreakStmt : Stmt
	{
		BreakStmt(const Location& l) : Stmt(l) { this->readableName = "break statement"; }
		~BreakStmt() { }

		virtual sst::Stmt* typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;
	};

	struct ContinueStmt : Stmt
	{
		ContinueStmt(const Location& l) : Stmt(l) { this->readableName = "continue statement"; }
		~ContinueStmt() { }

		virtual sst::Stmt* typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;
	};


	struct StaticStmt : Stmt
	{
		StaticStmt(Stmt* s) : Stmt(s->loc), actual(s) { this->readableName = "static declaration"; }
		~StaticStmt() { }

		virtual sst::Stmt* typecheck(sst::TypecheckState* fs, fir::Type* inf = 0) override { return this->actual->typecheck(fs, inf); }

		Stmt* actual = 0;
	};


	struct TypeDefn : Stmt
	{
		TypeDefn(const Location& l) : Stmt(l) { this->readableName = "type definition"; }
		~TypeDefn() { }

		virtual sst::Stmt* typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override = 0;
		VisibilityLevel visibility = VisibilityLevel::Internal;
	};

	struct StructDefn : TypeDefn
	{
		StructDefn(const Location& l) : TypeDefn(l) { this->readableName = "struct definition"; }
		~StructDefn() { }

		virtual sst::Stmt* typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;

		std::string name;
		std::map<std::string, TypeConstraints_t> generics;

		std::vector<VarDefn*> fields;
		std::vector<FuncDefn*> methods;
		std::vector<TypeDefn*> nestedTypes;
	};

	struct ClassDefn : TypeDefn
	{
		ClassDefn(const Location& l) : TypeDefn(l) { this->readableName = "class definition"; }
		~ClassDefn() { }

		virtual sst::Stmt* typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;

		std::string name;
		std::map<std::string, TypeConstraints_t> generics;

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

		virtual sst::Stmt* typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;

		struct Case
		{
			Location loc;
			std::string name;
			Expr* value = 0;
		};

		std::string name;
		std::vector<Case> cases;
		pts::Type* memberType = 0;
	};

	struct TypeExpr : Expr
	{
		TypeExpr(const Location& l, pts::Type* t) : Expr(l), type(t) { this->readableName = "<TYPE EXPRESSION>"; }
		~TypeExpr() { }

		virtual sst::Expr* typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;

		pts::Type* type = 0;
	};

	struct Ident : Expr
	{
		Ident(const Location& l, std::string n) : Expr(l), name(n) { this->readableName = "identifier"; }
		~Ident() { }

		virtual sst::Expr* typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;

		std::string name;
		bool traverseUpwards = true;
	};


	struct RangeExpr : Expr
	{
		RangeExpr(const Location& loc) : Expr(loc) { this->readableName = "range expression"; }
		~RangeExpr() { }

		virtual sst::Expr* typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;

		Expr* start = 0;
		Expr* end = 0;

		Expr* step = 0;

		bool halfOpen = false;
	};



	struct AllocOp : Expr
	{
		AllocOp(const Location& l) : Expr(l) { this->readableName = "alloc statement"; }
		~AllocOp() { }

		virtual sst::Expr* typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;

		pts::Type* allocTy = 0;
		std::vector<Expr*> counts;

		bool isRaw = false;
	};

	struct DeallocOp : Stmt
	{
		DeallocOp(const Location& l) : Stmt(l) { this->readableName = "free statement"; }
		~DeallocOp() { }

		virtual sst::Stmt* typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;
		Expr* expr = 0;
	};



	struct BinaryOp : Expr
	{
		BinaryOp(const Location& loc, std::string o, Expr* l, Expr* r) : Expr(loc), op(o), left(l), right(r) { this->readableName = "binary expression"; }
		~BinaryOp() { }

		virtual sst::Expr* typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;

		std::string op;

		Expr* left = 0;
		Expr* right = 0;
	};

	struct UnaryOp : Expr
	{
		UnaryOp(const Location& l) : Expr(l) { this->readableName = "unary expression"; }
		~UnaryOp() { }

		virtual sst::Expr* typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;

		std::string op;
		Expr* expr = 0;
		bool isPostfix = false;
	};

	struct AssignOp : Expr
	{
		AssignOp(const Location& l) : Expr(l) { this->readableName = "assignment statement"; }
		~AssignOp() { }

		virtual sst::Expr* typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;

		std::string op;

		Expr* left = 0;
		Expr* right = 0;
	};

	struct SubscriptOp : Expr
	{
		SubscriptOp(const Location& l) : Expr(l) { }
		~SubscriptOp() { }

		virtual sst::Expr* typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;

		Expr* expr = 0;
		Expr* inside = 0;
	};

	struct SliceOp : Expr
	{
		SliceOp(const Location& l) : Expr(l) { this->readableName = "slice expression"; }
		~SliceOp() { }

		virtual sst::Expr* typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;

		Expr* expr = 0;

		Expr* start = 0;
		Expr* end = 0;
	};

	struct SplatOp : Expr
	{
		SplatOp(const Location& l) : Expr(l) { this->readableName = "splat expression"; }
		~SplatOp() { }

		virtual sst::Expr* typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;

		Expr* expr = 0;
	};





	struct FunctionCall : Expr
	{
		FunctionCall(const Location& l, std::string n) : Expr(l), name(n) { this->readableName = "function call"; }
		~FunctionCall() { }

		virtual sst::Expr* typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;
		sst::Expr* typecheckWithArguments(sst::TypecheckState* fs, const std::vector<FnCallArgument>& args);

		std::string name;
		std::vector<std::pair<std::string, Expr*>> args;

		bool traverseUpwards = true;
	};

	struct ExprCall : Expr
	{
		ExprCall(const Location& l) : Expr(l) { this->readableName = "function call"; }
		~ExprCall() { }

		virtual sst::Expr* typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;
		sst::Expr* typecheckWithArguments(sst::TypecheckState* fs, const std::vector<FnCallArgument>& args);

		Expr* callee = 0;
		std::vector<std::pair<std::string, Expr*>> args;
	};



	struct DotOperator : Expr
	{
		DotOperator(const Location& loc, Expr* l, Expr* r) : Expr(loc), left(l), right(r) { this->readableName = "dot operator"; }
		~DotOperator() { }

		virtual sst::Expr* typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;

		Expr* left = 0;
		Expr* right = 0;
	};




	struct LitNumber : Expr
	{
		LitNumber(const Location& l, std::string n) : Expr(l), num(n) { this->readableName = "number literal"; }
		~LitNumber() { }

		virtual sst::Expr* typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;

		std::string num;
	};

	struct LitBool : Expr
	{
		LitBool(const Location& l, bool val) : Expr(l), value(val) { this->readableName = "boolean literal"; }
		~LitBool() { }

		virtual sst::Expr* typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;

		bool value = false;
	};

	struct LitString : Expr
	{
		LitString(const Location& l, std::string s, bool isc) : Expr(l), str(s), isCString(isc) { this->readableName = "string literal"; }
		~LitString() { }

		virtual sst::Expr* typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;

		std::string str;
		bool isCString = false;
	};

	struct LitNull : Expr
	{
		LitNull(const Location& l) : Expr(l) { this->readableName = "null literal"; }
		~LitNull() { }

		virtual sst::Expr* typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;
	};

	struct LitTuple : Expr
	{
		LitTuple(const Location& l, std::vector<Expr*> its) : Expr(l), values(its) { this->readableName = "tuple literal"; }
		~LitTuple() { }

		virtual sst::Expr* typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;

		std::vector<Expr*> values;
	};

	struct LitArray : Expr
	{
		LitArray(const Location& l, std::vector<Expr*> its) : Expr(l), values(its) { this->readableName = "array literal"; }
		~LitArray() { }

		virtual sst::Expr* typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;

		bool raw = false;
		std::vector<Expr*> values;
	};


	struct TopLevelBlock : Stmt
	{
		TopLevelBlock(const Location& l, std::string n) : Stmt(l), name(n) { this->readableName = "namespace"; }
		~TopLevelBlock() { }

		virtual sst::Stmt* typecheck(sst::TypecheckState* fs, fir::Type* inferred = 0) override;

		std::string name;
		std::vector<Stmt*> statements;
		VisibilityLevel visibility = VisibilityLevel::Internal;
	};
}






