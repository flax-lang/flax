// sst.h
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once
#include "defs.h"
#include "sst_expr.h"
#include "stcommon.h"

namespace fir
{
	struct Type;
	struct Function;
	struct FunctionType;
	struct ConstantValue;
}

namespace cgn
{
	struct CodegenState;
}

namespace sst
{
	struct StateTree;
	struct Block;

	struct HasBlocks
	{
		HasBlocks() { }
		virtual ~HasBlocks() { }
		virtual std::vector<Block*> getBlocks() = 0;
	};

	struct Defn : Stmt
	{
		Defn(const Location& l) : Stmt(l) { this->readableName = "definition"; }
		~Defn() { }

		Identifier id;
		fir::Type* type = 0;
		bool global = false;
		VisibilityLevel visibility = VisibilityLevel::Internal;
	};

	struct TypeDefn : Defn
	{
		TypeDefn(const Location& l) : Defn(l) { this->readableName = "type definition"; }
		~TypeDefn() { }
	};


	struct TypeExpr : Expr
	{
		TypeExpr(const Location& l, fir::Type* t) : Expr(l, t) { this->readableName = "<TYPE EXPRESSION>"; }
		~TypeExpr() { }

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) override;
	};

	struct RawValueExpr : Expr
	{
		RawValueExpr(const Location& l, fir::Type* t) : Expr(l, t) { this->readableName = "<RAW VALUE EXPRESSION>"; }
		~RawValueExpr() { }

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) override { return this->rawValue; }

		CGResult rawValue;
	};



	struct Block : Stmt
	{
		Block(const Location& l) : Stmt(l) { this->readableName = "block"; }
		~Block() { }

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) override;

		Location closingBrace;

		std::vector<std::string> scope;

		bool isSingleExpr = false;
		std::vector<Stmt*> statements;
		std::vector<Stmt*> deferred;
	};

	struct IfStmt : Stmt, HasBlocks
	{
		IfStmt(const Location& l) : Stmt(l) { this->readableName = "if statement"; }
		~IfStmt() { }

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) override;
		virtual std::vector<Block*> getBlocks() override;

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

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) override;

		Expr* value = 0;
		fir::Type* expectedType = 0;
	};

	struct WhileLoop : Stmt, HasBlocks
	{
		WhileLoop(const Location& l) : Stmt(l) { this->readableName = "while loop"; }
		~WhileLoop() { }

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) override;
		virtual std::vector<Block*> getBlocks() override;

		Expr* cond = 0;
		Block* body = 0;

		bool isDoVariant = false;
	};

	struct VarDefn;
	struct ForeachLoop : Stmt, HasBlocks
	{
		ForeachLoop(const Location& l) : Stmt(l) { this->readableName = "for loop"; }
		~ForeachLoop() { }

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) override;
		virtual std::vector<Block*> getBlocks() override;

		VarDefn* var = 0;
		Expr* array = 0;

		Block* body = 0;
	};


	struct BreakStmt : Stmt
	{
		BreakStmt(const Location& l) : Stmt(l) { this->readableName = "break statement"; }
		~BreakStmt() { }

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) override;
	};

	struct ContinueStmt : Stmt
	{
		ContinueStmt(const Location& l) : Stmt(l) { this->readableName = "continue statement"; }
		~ContinueStmt() { }

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) override;
	};



	struct SizeofOp : Expr
	{
		SizeofOp(const Location& l, fir::Type* t) : Expr(l, t) { this->readableName = "sizeof expression"; }
		~SizeofOp() { }

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) override;

		fir::Type* typeToSize = 0;
	};

	struct AllocOp : Expr
	{
		AllocOp(const Location& l, fir::Type* t) : Expr(l, t) { this->readableName = "alloc statement"; }
		~AllocOp() { }

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) override;

		fir::Type* elmType = 0;
		std::vector<Expr*> counts;

		bool isRaw = false;
	};

	struct DeallocOp : Stmt
	{
		DeallocOp(const Location& l) : Stmt(l) { this->readableName = "free statement"; }
		~DeallocOp() { }

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) override;
		Expr* expr = 0;
	};

	struct FunctionDefn;
	struct BinaryOp : Expr
	{
		BinaryOp(const Location& l, fir::Type* t) : Expr(l, t) { this->readableName = "binary expression"; }
		~BinaryOp() { }

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) override;

		Expr* left = 0;
		Expr* right = 0;
		std::string op;

		FunctionDefn* overloadedOpFunction = 0;
	};

	struct UnaryOp : Expr
	{
		UnaryOp(const Location& l, fir::Type* t) : Expr(l, t) { this->readableName = "unary expression"; }
		~UnaryOp() { }

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) override;

		Expr* expr = 0;
		std::string op;

		FunctionDefn* overloadedOpFunction = 0;
	};

	struct AssignOp : Expr
	{
		AssignOp(const Location& l);
		~AssignOp() { }

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) override;

		std::string op;

		Expr* left = 0;
		Expr* right = 0;
	};

	//* for the case where we assign to a tuple literal, to enable (a, b) = (b, a) (or really (a, b) = anything)
	struct TupleAssignOp : Expr
	{
		TupleAssignOp(const Location& l);
		~TupleAssignOp() { }

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) override;

		std::vector<Expr*> lefts;
		Expr* right = 0;
	};



	struct SubscriptOp : Expr
	{
		SubscriptOp(const Location& l, fir::Type* t) : Expr(l, t) { this->readableName = "subscript expression"; }
		~SubscriptOp() { }

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) override;

		Expr* expr = 0;
		Expr* inside = 0;

		// to assist safety checks, store the generated things
		fir::Value* cgSubscripteePtr = 0;
		fir::Value* cgSubscriptee = 0;
		fir::Value* cgIndex = 0;
	};


	struct SliceOp : Expr
	{
		SliceOp(const Location& l, fir::Type* t) : Expr(l, t) { this->readableName = "slice expression"; }
		~SliceOp() { }

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) override;

		Expr* expr = 0;
		Expr* begin = 0;
		Expr* end = 0;

		// to assist safety checks, store the generated things
		fir::Value* cgSubscripteePtr = 0;
		fir::Value* cgSubscriptee = 0;
		fir::Value* cgBegin = 0;
		fir::Value* cgEnd = 0;
	};

	struct FunctionCall : Expr
	{
		FunctionCall(const Location& l, fir::Type* t) : Expr(l, t) { this->readableName = "function call"; }
		~FunctionCall() { }

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) override;

		std::string name;
		Defn* target = 0;
		std::vector<FnCallArgument> arguments;
		bool isImplicitMethodCall = false;
	};

	struct ExprCall : Expr
	{
		ExprCall(const Location& l, fir::Type* t) : Expr(l, t) { this->readableName = "function call"; }
		~ExprCall() { }

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) override;

		Expr* callee = 0;
		std::vector<Expr*> arguments;
	};


	struct StructDefn;
	struct ClassDefn;
	struct StructConstructorCall : Expr
	{
		StructConstructorCall(const Location& l, fir::Type* t) : Expr(l, t) { this->readableName = "struct constructor call"; }
		~StructConstructorCall() { }

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) override;

		StructDefn* target = 0;
		std::vector<FnCallArgument> arguments;
	};

	struct ClassConstructorCall : Expr
	{
		ClassConstructorCall(const Location& l, fir::Type* t) : Expr(l, t) { this->readableName = "class constructor call"; }
		~ClassConstructorCall() { }

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) override;

		ClassDefn* classty = 0;
		FunctionDefn* target = 0;
		std::vector<FnCallArgument> arguments;
	};




	struct VarDefn;
	struct VarRef : Expr
	{
		VarRef(const Location& l, fir::Type* t) : Expr(l, t) { this->readableName = "identifier"; }
		~VarRef() { }

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) override;

		std::string name;
		Defn* def = 0;
	};

	struct ScopeExpr : Expr
	{
		ScopeExpr(const Location& l, fir::Type* t) : Expr(l, t) { this->readableName = "<SCOPE EXPRESSION>"; }
		~ScopeExpr() { }

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) override;

		std::vector<std::string> scope;
	};

	struct FieldDotOp : Expr
	{
		FieldDotOp(const Location& l, fir::Type* t) : Expr(l, t) { this->readableName = "field access"; }
		~FieldDotOp() { }

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) override;

		Expr* lhs = 0;
		std::string rhsIdent;
		bool isMethodRef = false;
	};

	struct MethodDotOp : Expr
	{
		MethodDotOp(const Location& l, fir::Type* t) : Expr(l, t) { this->readableName = "method call"; }
		~MethodDotOp() { }

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) override;

		Expr* lhs = 0;
		Expr* call = 0;
	};

	struct TupleDotOp : Expr
	{
		TupleDotOp(const Location& l, fir::Type* t) : Expr(l, t) { this->readableName = "tuple access"; }
		~TupleDotOp() { }

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) override;

		Expr* lhs = 0;
		size_t index = 0;
	};

	struct BuiltinDotOp : Expr
	{
		BuiltinDotOp(const Location& l, fir::Type* t) : Expr(l, t) { this->readableName = "dot operator"; }
		~BuiltinDotOp() { }

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) override;

		Expr* lhs = 0;
		std::string name;

		bool isFunctionCall = false;
		std::vector<Expr*> args;
	};

	struct EnumDefn;
	struct EnumDotOp : Expr
	{
		EnumDotOp(const Location& l, fir::Type* t) : Expr(l, t) { this->readableName = "enum case access"; }
		~EnumDotOp() { }

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) override;

		std::string caseName;
		EnumDefn* enumeration = 0;
	};



	struct LiteralNumber : Expr
	{
		LiteralNumber(const Location& l, fir::Type* t) : Expr(l, t) { this->readableName = "number literal"; }
		~LiteralNumber() { }

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) override;

		mpfr::mpreal number;
	};

	struct LiteralString : Expr
	{
		LiteralString(const Location& l, fir::Type* t) : Expr(l, t) { this->readableName = "string literal"; }
		~LiteralString() { }

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) override;

		std::string str;
		bool isCString = false;
	};

	struct LiteralNull : Expr
	{
		LiteralNull(const Location& l, fir::Type* t) : Expr(l, t) { this->readableName = "null literal"; }
		~LiteralNull() { }

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) override;
	};

	struct LiteralBool : Expr
	{
		LiteralBool(const Location& l, fir::Type* t) : Expr(l, t) { this->readableName = "boolean literal"; }
		~LiteralBool() { }

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) override;

		bool value = false;
	};

	struct LiteralTuple : Expr
	{
		LiteralTuple(const Location& l, fir::Type* t) : Expr(l, t) { this->readableName = "tuple literal"; }
		~LiteralTuple() { }

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) override;

		std::vector<Expr*> values;
	};

	struct LiteralArray : Expr
	{
		LiteralArray(const Location& l, fir::Type* t) : Expr(l, t) { this->readableName = "array literal"; }
		~LiteralArray() { }

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) override;

		std::vector<Expr*> values;
	};



	struct RangeExpr : Expr
	{
		RangeExpr(const Location& l, fir::Type* t) : Expr(l, t) { this->readableName = "range expression"; }
		~RangeExpr() { }

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) override;

		Expr* start = 0;
		Expr* end = 0;

		Expr* step = 0;
		bool halfOpen = false;
	};






	struct TreeDefn : Defn
	{
		TreeDefn(const Location& l) : Defn(l) { this->readableName = "<TREE DEFINITION>"; }
		~TreeDefn() { }

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) override;

		StateTree* tree = 0;
	};

	struct NamespaceDefn : Stmt
	{
		NamespaceDefn(const Location& l) : Stmt(l) { this->readableName = "namespace"; }
		~NamespaceDefn() { }

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) override;

		std::string name;
		std::vector<Stmt*> statements;
	};

	struct ArgumentDefn : Defn
	{
		ArgumentDefn(const Location& l) : Defn(l) { this->readableName = "<ARGUMENT DEFINITION>"; }
		~ArgumentDefn() { }

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) override;
	};

	struct VarDefn : Defn
	{
		VarDefn(const Location& l) : Defn(l) { this->readableName = "variable definition"; }
		~VarDefn() { }

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) override;

		Expr* init = 0;
		bool immutable = false;
	};

	struct FunctionDecl : Defn
	{
		struct Param
		{
			std::string name;
			Location loc;
			fir::Type* type = 0;
		};

		std::vector<Param> params;
		fir::Type* returnType = 0;

		bool isEntry = false;
		bool noMangle = false;
		bool isVarArg = false;

		protected:
		FunctionDecl(const Location& l) : Defn(l) { this->readableName = "function declaration"; }
		~FunctionDecl() { }
	};

	struct FunctionDefn : FunctionDecl
	{
		FunctionDefn(const Location& l) : FunctionDecl(l) { this->readableName = "function definition"; }
		~FunctionDefn() { }

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) override;

		std::vector<ArgumentDefn*> arguments;

		Block* body = 0;
		bool needReturnVoid = false;
		fir::Type* parentTypeForMethod = 0;
	};

	struct ForeignFuncDefn : FunctionDecl
	{
		ForeignFuncDefn(const Location& l) : FunctionDecl(l) { this->readableName = "foreign function definition"; }
		~ForeignFuncDefn() { }

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) override;
	};

	struct OperatorOverloadDefn : FunctionDefn
	{
		OperatorOverloadDefn(const Location& l) : FunctionDefn(l) { this->readableName = "operator overload definition"; }
		~OperatorOverloadDefn() { }

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) override;
	};



	struct DecompDefn : Stmt
	{
		DecompDefn(const Location& l) : Stmt(l) { this->readableName = "destructuring variable definition"; }
		~DecompDefn() { }

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) override;

		Expr* init = 0;
		bool immutable = false;
		DecompMapping bindings;
	};



	struct StructDefn : TypeDefn
	{
		StructDefn(const Location& l) : TypeDefn(l) { this->readableName = "struct definition"; }
		~StructDefn() { }

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) override;

		std::vector<VarDefn*> fields;
		std::vector<FunctionDefn*> methods;
		std::vector<TypeDefn*> nestedTypes;
	};


	struct ClassDefn : StructDefn
	{
		ClassDefn(const Location& l) : StructDefn(l) { this->readableName = "class definition"; }
		~ClassDefn() { }

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) override;

		fir::Function* inlineInitFunction = 0;
		std::vector<FunctionDefn*> initialisers;

		std::vector<VarDefn*> staticFields;
		std::vector<FunctionDefn*> staticMethods;
	};


	struct EnumCaseDefn : Defn
	{
		EnumCaseDefn(const Location& l) : Defn(l) { this->readableName = "enum case definition"; }
		~EnumCaseDefn() { }

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) override;

		Expr* val = 0;
		size_t index = 0;
		EnumDefn* parentEnum = 0;
		fir::ConstantValue* value = 0;
	};

	struct EnumDefn : TypeDefn
	{
		EnumDefn(const Location& l) : TypeDefn(l) { this->readableName = "enum definition"; }
		~EnumDefn() { }

		virtual CGResult _codegen(cgn::CodegenState* cs, fir::Type* inferred = 0) override;

		fir::Type* memberType = 0;
		std::unordered_map<std::string, EnumCaseDefn*> cases;
	};
}

















