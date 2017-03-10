// ast.h
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once

#include "typeinfo.h"
#include "defs.h"
#include "ir/identifier.h"

#include "ir/type.h"




namespace pts
{
	struct Type;
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

		Subscript,
		Slice,

		UserDefined
	};

	enum class FFIType
	{
		C
	};

	extern uint64_t Attr_Invalid;
	extern uint64_t Attr_NoMangle;
	extern uint64_t Attr_VisPublic;
	extern uint64_t Attr_VisInternal;
	extern uint64_t Attr_VisPrivate;
	extern uint64_t Attr_ForceMangle;
	extern uint64_t Attr_NoAutoInit;
	extern uint64_t Attr_PackedStruct;
	extern uint64_t Attr_StrongTypeAlias;
	extern uint64_t Attr_RawString;
	extern uint64_t Attr_Override;
	extern uint64_t Attr_CommutativeOp;

	enum class ResultType { Normal, BreakCodegen };
	enum class ValueKind { RValue, LValue };
	struct Result_t
	{
		Result_t(fir::Value* val, fir::Value* ptr, ResultType rt, ValueKind vk) : value(val), pointer(ptr), type(rt), valueKind(vk) { }
		Result_t(fir::Value* val, fir::Value* ptr, ResultType rt) : value(val), pointer(ptr), type(rt), valueKind(ValueKind::RValue) { }
		Result_t(fir::Value* val, fir::Value* ptr, ValueKind vk) : value(val), pointer(ptr), type(ResultType::Normal), valueKind(vk) { }
		Result_t(fir::Value* val, fir::Value* ptr) : value(val), pointer(ptr), type(ResultType::Normal), valueKind(ValueKind::RValue) { }

		operator std::tuple<fir::Value*&, fir::Value*&>()
		{
			return std::tuple<fir::Value*&, fir::Value*&>(this->value, this->pointer);
		}

		operator std::tuple<fir::Value*&, fir::Value*&, ResultType&>()
		{
			return std::tuple<fir::Value*&, fir::Value*&, ResultType&>(this->value, this->pointer, this->type);
		}

		operator std::tuple<fir::Value*&, ResultType&>()
		{
			return std::tuple<fir::Value*&, ResultType&>(this->value, this->type);
		}

		operator std::tuple<fir::Value*&, fir::Value*&, ValueKind&>()
		{
			return std::tuple<fir::Value*&, fir::Value*&, ValueKind&>(this->value, this->pointer, this->valueKind);
		}

		operator std::tuple<fir::Value*&, fir::Value*&, ResultType&, ValueKind&>()
		{
			return std::tuple<fir::Value*&, fir::Value*&, ResultType&, ValueKind&>(this->value, this->pointer, this->type, this->valueKind);
		}


		fir::Value* value;
		fir::Value* pointer;

		ResultType type;
		ValueKind valueKind;
	};






	struct Expr
	{
		explicit Expr(const Parser::Pin& pos) : pin(pos) { }
		virtual ~Expr() { }
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Type* extratype, fir::Value* target) = 0;
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, fir::Type* extratype = 0, bool allowFail = false) = 0;

		virtual Result_t codegen(Codegen::CodegenInstance* cgi);
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* target);
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Type* extratype);

		virtual bool isBreaking() { return false; }

		bool didCodegen = false;
		uint64_t attribs = 0;
		Parser::Pin pin;

		// ExprType type;
		pts::Type* ptype = 0;
	};

	struct DummyExpr : Expr
	{
		using Expr::codegen;

		explicit DummyExpr(const Parser::Pin& pos) : Expr(pos) { }
		~DummyExpr();

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Type* extratype, fir::Value* target) override { return Result_t(0, 0); }
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, fir::Type* extratype = 0, bool allowFail = false) override;
	};


	struct Number : Expr
	{
		using Expr::codegen;

		~Number();
		Number(const Parser::Pin& pos, std::string s) : Expr(pos), str(s) { }
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Type* extratype, fir::Value* target) override;
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, fir::Type* extratype = 0, bool allowFail = false) override;

		std::string str;
	};

	struct BoolVal : Expr
	{
		using Expr::codegen;

		~BoolVal();
		BoolVal(const Parser::Pin& pos, bool val) : Expr(pos), val(val) { }

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Type* extratype, fir::Value* target) override;
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, fir::Type* extratype = 0, bool allowFail = false) override;

		bool val = false;
	};

	struct NullVal : Expr
	{
		using Expr::codegen;

		~NullVal();
		NullVal(const Parser::Pin& pos) : Expr(pos) { }

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Type* extratype, fir::Value* target) override;
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, fir::Type* extratype = 0, bool allowFail = false) override;
	};





	struct VarRef : Expr
	{
		using Expr::codegen;

		~VarRef();
		VarRef(const Parser::Pin& pos, std::string name) : Expr(pos), name(name) { }

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Type* extratype, fir::Value* target) override;
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, fir::Type* extratype = 0, bool allowFail = false) override;

		std::string name;
	};



	struct VarDecl : Expr
	{
		using Expr::codegen;

		~VarDecl();
		VarDecl(const Parser::Pin& pos, std::string name, bool immut) : Expr(pos), immutable(immut)
		{
			ident.name = name;
			ident.kind = IdKind::Variable;
		}

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Type* extratype, fir::Value* target) override;
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, fir::Type* extratype = 0, bool allowFail = false) override;

		fir::Value* doInitialValue(Codegen::CodegenInstance* cgi, Codegen::TypePair_t* type, fir::Value* val, fir::Value* valptr, fir::Value* storage, bool shouldAddToSymtab, ValueKind vk);

		void inferType(Codegen::CodegenInstance* cgi);

		Identifier ident;

		bool immutable = false;

		bool isStatic = false;
		bool isGlobal = false;
		bool disableAutoInit = false;
		Expr* initVal = 0;
		fir::Type* concretisedType = 0;
	};

	struct TupleDecompDecl : Expr
	{
		using Expr::codegen;

		struct Mapping
		{
			bool isRecursive = false;

			Parser::Pin pos;
			std::string name;
			std::vector<Mapping> inners;
		};


		~TupleDecompDecl();
		TupleDecompDecl(const Parser::Pin& pos) : Expr(pos) { }

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Type* extratype, fir::Value* target) override;
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, fir::Type* extratype = 0, bool allowFail = false) override;

		void decomposeWithRhs(Codegen::CodegenInstance* cgi, fir::Value* rhs, fir::Value* rhsptr, ValueKind vk);

		bool immutable = false;
		Mapping mapping;
		Expr* rightSide = 0;
	};




	struct ArrayDecompDecl : Expr
	{
		using Expr::codegen;

		~ArrayDecompDecl();
		ArrayDecompDecl(const Parser::Pin& pos) : Expr(pos) { }

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Type* extratype, fir::Value* target) override;
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, fir::Type* extratype = 0, bool allowFail = false) override;

		void decomposeWithRhs(Codegen::CodegenInstance* cgi, fir::Value* rhs, fir::Value* rhsptr, ValueKind vk);

		bool immutable = false;

		Expr* rightSide = 0;
		std::unordered_map<size_t, std::pair<std::string, Parser::Pin>> mapping;
	};




	struct BracedBlock;
	struct ComputedProperty : VarDecl
	{
		using Expr::codegen;

		~ComputedProperty();
		ComputedProperty(const Parser::Pin& pos, std::string name) : VarDecl(pos, name, false) { }

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Type* extratype, fir::Value* target) override;
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, fir::Type* extratype = 0, bool allowFail = false) override;

		FuncDecl* getterFunc = 0;
		FuncDecl* setterFunc = 0;
		std::string setterArgName;
		BracedBlock* getter = 0;
		BracedBlock* setter = 0;

		fir::Function* getterFFn = 0;
		fir::Function* setterFFn = 0;
	};

	struct BinOp : Expr
	{
		using Expr::codegen;

		~BinOp();
		BinOp(const Parser::Pin& pos, Expr* lhs, ArithmeticOp operation, Expr* rhs) : Expr(pos), left(lhs), right(rhs), op(operation) { }

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Type* extratype, fir::Value* target) override;
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, fir::Type* extratype = 0, bool allowFail = false) override;

		Expr* left = 0;
		Expr* right = 0;

		ArithmeticOp op = ArithmeticOp::Invalid;
	};

	struct StructBase;
	struct FuncDecl : Expr
	{
		using Expr::codegen;

		~FuncDecl();
		FuncDecl(const Parser::Pin& pos, std::string id, std::vector<VarDecl*> params, pts::Type* ret) : Expr(pos), params(params)
		{
			this->ident.name = id;
			this->ident.kind = IdKind::Function;

			this->ptype = ret;
		}

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Type* extratype, fir::Value* target) override;
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, fir::Type* extratype = 0, bool allowFail = false) override;

		Result_t generateDeclForGenericFunction(Codegen::CodegenInstance* cgi, std::map<std::string, fir::Type*> types);
		Result_t generateDeclForGenericFunctionUsingFunctionType(Codegen::CodegenInstance* cgi, fir::FunctionType* ft);

		Parser::Pin returnTypePos;

		bool isCStyleVarArg = false;

		bool isVariadic = false;
		// std::string variadicStrType;
		// fir::Type* variadicType = 0;
		fir::Function* generatedFunc = 0;

		bool isFFI = false;
		bool isStatic = false;

		StructBase* parentClass = 0;
		FFIType ffiType = FFIType::C;

		Identifier ident;

		std::vector<VarDecl*> params;
		std::map<std::string, TypeConstraints_t> genericTypes;


	};








	struct DeferredExpr;
	struct BracedBlock : Expr
	{
		using Expr::codegen;

		explicit BracedBlock(const Parser::Pin& pos) : Expr(pos) { }
		~BracedBlock();

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Type* extratype, fir::Value* target) override;
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, fir::Type* extratype = 0, bool allowFail = false) override;

		std::vector<Expr*> statements;
		std::vector<DeferredExpr*> deferredStatements;
	};

	struct Func : Expr
	{
		using Expr::codegen;

		~Func();
		Func(const Parser::Pin& pos, FuncDecl* funcdecl, BracedBlock* block) : Expr(pos), decl(funcdecl), block(block) { }

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Type* extratype, fir::Value* target) override;
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, fir::Type* extratype = 0, bool allowFail = false) override;

		FuncDecl* decl = 0;
		BracedBlock* block = 0;
	};

	struct FuncCall : Expr
	{
		using Expr::codegen;

		~FuncCall();
		FuncCall(const Parser::Pin& pos, std::string target, std::vector<Expr*> args) : Expr(pos), name(target), params(args) { }

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Type* extratype, fir::Value* target) override;
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, fir::Type* extratype = 0, bool allowFail = false) override;

		std::string name;
		std::vector<Expr*> params;

		Codegen::Resolved_t cachedResolveTarget;
	};

	struct Return : Expr
	{
		using Expr::codegen;

		~Return();
		Return(const Parser::Pin& pos, Expr* e) : Expr(pos), val(e) { }

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Type* extratype, fir::Value* target) override;
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, fir::Type* extratype = 0, bool allowFail = false) override;
		virtual bool isBreaking() override { return true; }

		Expr* val = 0;
		fir::Value* actualReturnValue = 0;
	};

	struct Import : Expr
	{
		using Expr::codegen;

		~Import();
		Import(const Parser::Pin& pos, std::string name) : Expr(pos), module(name) { }

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Type* extratype, fir::Value* target) override { return Result_t(0, 0); }
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, fir::Type* extratype = 0, bool allowFail = false) override { return 0; };

		std::string module;
	};

	struct ForeignFuncDecl : Expr
	{
		using Expr::codegen;

		~ForeignFuncDecl();
		ForeignFuncDecl(const Parser::Pin& pos, FuncDecl* func) : Expr(pos), decl(func) { }

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Type* extratype, fir::Value* target) override;
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, fir::Type* extratype = 0, bool allowFail = false) override;

		FuncDecl* decl = 0;
	};

	struct DeferredExpr : Expr
	{
		using Expr::codegen;

		DeferredExpr(const Parser::Pin& pos, Expr* e) : Expr(pos), expr(e) { }
		~DeferredExpr();

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Type* extratype, fir::Value* target) override;
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, fir::Type* extratype = 0, bool allowFail = false) override;

		Expr* expr = 0;
	};

	struct BreakableBracedBlock : Expr
	{
		using Expr::codegen;

		explicit BreakableBracedBlock(const Parser::Pin& pos, BracedBlock* _body) : Expr(pos), body(_body) { }
		~BreakableBracedBlock();

		BracedBlock* body = 0;
	};

	struct IfStmt : Expr
	{
		using Expr::codegen;

		~IfStmt();
		IfStmt(const Parser::Pin& pos, std::vector<std::tuple<Expr*, BracedBlock*, std::vector<Expr*>>> cases, BracedBlock* ecase)
			: Expr(pos), final(ecase), cases(cases) { }

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Type* extratype, fir::Value* target) override;
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, fir::Type* extratype = 0, bool allowFail = false) override;

		BracedBlock* final = 0;
		std::vector<std::tuple<Expr*, BracedBlock*, std::vector<Expr*>>> cases;
	};

	struct WhileLoop : BreakableBracedBlock
	{
		using Expr::codegen;

		~WhileLoop();
		WhileLoop(const Parser::Pin& pos, Expr* _cond, BracedBlock* _body, bool dowhile) : BreakableBracedBlock(pos, _body),
			cond(_cond), isDoWhileVariant(dowhile) { }

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Type* extratype, fir::Value* target) override;
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, fir::Type* extratype = 0, bool allowFail = false) override;

		Expr* cond = 0;
		bool isDoWhileVariant = false;
	};

	struct ForLoop : BreakableBracedBlock
	{
		using Expr::codegen;

		~ForLoop();
		ForLoop(const Parser::Pin& pos, BracedBlock* _body) : BreakableBracedBlock(pos, _body) { }

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Type* extratype, fir::Value* target) override;
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, fir::Type* extratype = 0, bool allowFail = false) override;


		Expr* init = 0;
		std::vector<Expr*> incrs;

		Expr* cond = 0;
	};

	struct Break : Expr
	{
		using Expr::codegen;

		~Break();
		explicit Break(const Parser::Pin& pos) : Expr(pos) { }

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Type* extratype, fir::Value* target) override;
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, fir::Type* extratype = 0, bool allowFail = false) override;
		virtual bool isBreaking() override { return true; }
	};

	struct Continue : Expr
	{
		using Expr::codegen;

		~Continue();
		explicit Continue(const Parser::Pin& pos) : Expr(pos) { }

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Type* extratype, fir::Value* target) override;
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, fir::Type* extratype = 0, bool allowFail = false) override;
		virtual bool isBreaking() override { return true; }
	};

	struct UnaryOp : Expr
	{
		using Expr::codegen;

		~UnaryOp();
		UnaryOp(const Parser::Pin& pos, ArithmeticOp op, Expr* expr) : Expr(pos), op(op), expr(expr) { }

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Type* extratype, fir::Value* target) override;
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, fir::Type* extratype = 0, bool allowFail = false) override;

		ArithmeticOp op;
		Expr* expr = 0;
	};

	// fuck
	struct OpOverload : Expr
	{
		using Expr::codegen;

		enum class OperatorKind
		{
			Invalid,

			PrefixUnary,
			PostfixUnary,

			CommBinary,
			NonCommBinary,
		};

		~OpOverload();
		OpOverload(const Parser::Pin& pos, ArithmeticOp op) : Expr(pos), op(op) { }

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Type* extratype, fir::Value* target) override;
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, fir::Type* extratype = 0, bool allowFail = false) override;

		Result_t codegenOp(Codegen::CodegenInstance* cgi, std::vector<fir::Type*> args);

		ArithmeticOp op = ArithmeticOp::Invalid;
		OperatorKind kind = OperatorKind::Invalid;

		Func* func = 0;
		fir::Function* lfunc = 0;
	};


	struct SubscriptOpOverload : Expr
	{
		using Expr::codegen;

		~SubscriptOpOverload();
		SubscriptOpOverload(const Parser::Pin& pos) : Expr(pos) { }

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Type* extratype, fir::Value* target) override;
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, fir::Type* extratype = 0, bool allowFail = false) override;

		std::string setterArgName;

		FuncDecl* decl = 0;

		BracedBlock* getterBody = 0;
		BracedBlock* setterBody = 0;

		fir::Function* getterFunc = 0;
		fir::Function* setterFunc = 0;

		Func* getterFn = 0;
		Func* setterFn = 0;
	};

	struct AssignOpOverload : Expr
	{
		using Expr::codegen;

		~AssignOpOverload();
		AssignOpOverload(const Parser::Pin& pos, ArithmeticOp o) : Expr(pos), op(o) { }

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Type* extratype, fir::Value* target) override;
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, fir::Type* extratype = 0, bool allowFail = false) override;

		Func* func = 0;
		ArithmeticOp op;
		fir::Function* lfunc = 0;
	};


	struct StructBase : Expr
	{
		using Expr::codegen;

		virtual ~StructBase();
		StructBase(const Parser::Pin& pos, std::string name) : Expr(pos)
		{
			this->ident.name = name;
			this->ident.kind = IdKind::Struct;
		}

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Type* extratype, fir::Value* target) override = 0;
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, fir::Type* extratype = 0, bool allowFail = false) override = 0;
		virtual fir::Type* createType(Codegen::CodegenInstance* cgi) = 0;

		bool didCreateType = false;
		fir::Type* createdType = 0;

		Identifier ident;

		std::vector<VarDecl*> members;
		std::vector<fir::Function*> initFuncs;

		fir::Function* defaultInitialiser;

		std::vector<std::pair<StructBase*, fir::Type*>> nestedTypes;
	};

	struct ClassDef : StructBase
	{
		using Expr::codegen;

		~ClassDef();
		ClassDef(const Parser::Pin& pos, std::string name) : StructBase(pos, name) { }

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Type* extratype, fir::Value* target) override;
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, fir::Type* extratype = 0, bool allowFail = false) override;
		virtual fir::Type* createType(Codegen::CodegenInstance* cgi) override;

		std::vector<Func*> funcs;
		std::vector<fir::Function*> lfuncs;
		std::vector<ComputedProperty*> cprops;
		std::vector<std::string> protocolstrs;
		std::vector<OpOverload*> operatorOverloads;
		std::vector<AssignOpOverload*> assignmentOverloads;
		std::vector<SubscriptOpOverload*> subscriptOverloads;
		std::unordered_map<Func*, fir::Function*> functionMap;

		std::vector<ProtocolDef*> conformedProtocols;
	};


	struct Root;
	struct ExtensionDef : ClassDef
	{
		using Expr::codegen;

		~ExtensionDef();
		ExtensionDef(const Parser::Pin& pos, std::string name) : ClassDef(pos, name) { }

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Type* extratype, fir::Value* target) override;
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, fir::Type* extratype = 0, bool allowFail = false) override;
		virtual fir::Type* createType(Codegen::CodegenInstance* cgi) override;

		bool isDuplicate = false;
		Root* parentRoot = 0;
	};

	struct ProtocolDef : Expr
	{
		using Expr::codegen;

		~ProtocolDef();
		ProtocolDef(const Parser::Pin& pos, std::string name) : Expr(pos)
		{
			this->ident.name = name;
			this->ident.kind = IdKind::Struct;
		}


		fir::Type* createType(Codegen::CodegenInstance* cgi);
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, fir::Type* extratype = 0, bool allowFail = false) override;
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Type* extratype, fir::Value* target) override;

		bool checkTypeConformity(Codegen::CodegenInstance* cgi, fir::Type* type);
		void assertTypeConformity(Codegen::CodegenInstance* cgi, fir::Type* type);

		Identifier ident;

		std::vector<std::string> protocolstrs;

		std::vector<Func*> funcs;
		std::vector<OpOverload*> operatorOverloads;
		std::vector<AssignOpOverload*> assignmentOverloads;
		std::vector<SubscriptOpOverload*> subscriptOverloads;
	};



	struct StructDef : StructBase
	{
		using Expr::codegen;

		~StructDef();
		StructDef(const Parser::Pin& pos, std::string name) : StructBase(pos, name) { }

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Type* extratype, fir::Value* target) override;
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, fir::Type* extratype = 0, bool allowFail = false) override;
		virtual fir::Type* createType(Codegen::CodegenInstance* cgi) override;

		bool packed = false;
	};

	struct EnumDef : StructBase
	{
		using Expr::codegen;

		~EnumDef();
		EnumDef(const Parser::Pin& pos, std::string name) : StructBase(pos, name) { }

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Type* extratype, fir::Value* target) override;
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, fir::Type* extratype = 0, bool allowFail = false) override;
		virtual fir::Type* createType(Codegen::CodegenInstance* cgi) override;

		std::vector<std::pair<std::string, Expr*>> cases;
		bool isStrong = false;
	};

	struct Tuple : StructBase
	{
		using Expr::codegen;

		~Tuple();
		Tuple(const Parser::Pin& pos, std::vector<Expr*> _values) : StructBase(pos, ""), values(_values) { }

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Type* extratype, fir::Value* target) override;
		virtual fir::TupleType* getType(Codegen::CodegenInstance* cgi, fir::Type* extratype = 0, bool allowFail = false) override;
		virtual fir::Type* createType(Codegen::CodegenInstance* cgi) override;

		std::vector<Expr*> values;
		std::vector<fir::Type*> ltypes;

		fir::TupleType* createdType = 0;
	};










	enum class MAType
	{
		Invalid,
		LeftStatic,
		LeftVariable,
		LeftFunctionCall
	};

	struct MemberAccess : Expr
	{
		using Expr::codegen;

		~MemberAccess();
		MemberAccess(const Parser::Pin& pos, Expr* _left, Expr* _right) : Expr(pos), left(_left), right(_right) { }

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Type* extratype, fir::Value* target) override;
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, fir::Type* extratype = 0, bool allowFail = false) override;

		bool disableStaticChecking = false;
		Result_t cachedCodegenResult = Result_t(0, 0);
		Expr* left = 0;
		Expr* right = 0;

		MAType matype = MAType::Invalid;
	};




	struct NamespaceDecl : Expr
	{
		using Expr::codegen;

		~NamespaceDecl();
		NamespaceDecl(const Parser::Pin& pos, std::string _name, BracedBlock* inside) : Expr(pos), innards(inside), name(_name) { }

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Type* extratype, fir::Value* target) override { return Result_t(0, 0); }
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, fir::Type* extratype = 0, bool allowFail = false) override { return 0; };

		void codegenPass(Codegen::CodegenInstance* cgi, int pass);

		std::vector<NamespaceDecl*> namespaces;
		BracedBlock* innards = 0;
		std::string name;
	};



	struct Range : Expr
	{
		using Expr::codegen;

		~Range();
		Range(const Parser::Pin& pos, Expr* s, Expr* e, bool half) : Expr(pos), start(s), end(e), isHalfOpen(half) { }

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Type* extratype, fir::Value* target) override;
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, fir::Type* extratype = 0, bool allowFail = false) override;

		Expr* start = 0;
		Expr* end = 0;

		bool isHalfOpen = false;
	};



	struct ArrayIndex : Expr
	{
		using Expr::codegen;

		~ArrayIndex();
		ArrayIndex(const Parser::Pin& pos, Expr* v, Expr* index) : Expr(pos), arr(v), index(index) { }

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Type* extratype, fir::Value* target) override;
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, fir::Type* extratype = 0, bool allowFail = false) override;

		Expr* arr = 0;
		Expr* index = 0;
	};

	struct ArraySlice : Expr
	{
		using Expr::codegen;

		~ArraySlice();
		ArraySlice(const Parser::Pin& pos, Expr* arr, Expr* st, Expr* ed) : Expr(pos), arr(arr), start(st), end(ed) { }

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Type* extratype, fir::Value* target) override;
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, fir::Type* extratype = 0, bool allowFail = false) override;

		Expr* arr = 0;
		Expr* start = 0;
		Expr* end = 0;
	};

	struct StringLiteral : Expr
	{
		using Expr::codegen;

		~StringLiteral();
		StringLiteral(const Parser::Pin& pos, std::string str) : Expr(pos), str(str) { }

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Type* extratype, fir::Value* target) override;
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, fir::Type* extratype = 0, bool allowFail = false) override;

		bool isRaw = false;
		std::string str;
	};

	struct ArrayLiteral : Expr
	{
		using Expr::codegen;

		~ArrayLiteral();
		ArrayLiteral(const Parser::Pin& pos, std::vector<Expr*> values) : Expr(pos), values(values) { }

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Type* extratype, fir::Value* target) override;
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, fir::Type* extratype = 0, bool allowFail = false) override;

		std::vector<Expr*> values;
	};

	struct TypeAlias : StructBase
	{
		using Expr::codegen;

		~TypeAlias();
		TypeAlias(const Parser::Pin& pos, std::string _alias, pts::Type* _origType) : StructBase(pos, _alias), origType(_origType) { }

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Type* extratype, fir::Value* target) override;
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, fir::Type* extratype = 0, bool allowFail = false) override;
		virtual fir::Type* createType(Codegen::CodegenInstance* cgi) override;

		bool isStrong = false;
		pts::Type* origType;
	};

	struct Alloc : Expr
	{
		using Expr::codegen;

		~Alloc();
		explicit Alloc(const Parser::Pin& pos) : Expr(pos) { }

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Type* extratype, fir::Value* target) override;
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, fir::Type* extratype = 0, bool allowFail = false) override;

		std::vector<Expr*> counts;
		std::vector<Expr*> params;
	};

	struct Dealloc : Expr
	{
		using Expr::codegen;

		~Dealloc();
		Dealloc(const Parser::Pin& pos, Expr* _expr) : Expr(pos), expr(_expr) { }

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Type* extratype, fir::Value* target) override;
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, fir::Type* extratype = 0, bool allowFail = false) override;

		Expr* expr = 0;
	};

	struct Typeof : Expr
	{
		using Expr::codegen;

		~Typeof();
		Typeof(const Parser::Pin& pos, Expr* _inside) : Expr(pos), inside(_inside) { }

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Type* extratype, fir::Value* target) override;
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, fir::Type* extratype = 0, bool allowFail = false) override;

		Expr* inside = 0;
	};


	struct Typeid : Expr
	{
		using Expr::codegen;

		~Typeid();
		Typeid(const Parser::Pin& pos, Expr* _inside) : Expr(pos), inside(_inside) { }

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Type* extratype, fir::Value* target) override;
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, fir::Type* extratype = 0, bool allowFail = false) override;

		Expr* inside = 0;
	};


	struct PostfixUnaryOp : Expr
	{
		using Expr::codegen;

		enum class Kind
		{
			Invalid,
			Increment,
			Decrement
		};

		~PostfixUnaryOp();
		PostfixUnaryOp(const Parser::Pin& pos, Expr* e, Kind k) : Expr(pos), kind(k), expr(e) { }

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Type* extratype, fir::Value* target) override;
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, fir::Type* extratype = 0, bool allowFail = false) override;

		Kind kind;
		Expr* expr = 0;
		std::vector<Expr*> args;
	};

	struct Root : Expr
	{
		using Expr::codegen;

		Root() : Expr(Parser::Pin()) { }
		~Root();

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Type* extratype, fir::Value* target) override;
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, fir::Type* extratype = 0, bool allowFail = false) override { return 0; };

		Codegen::FunctionTree* rootFuncStack = new Codegen::FunctionTree("", 0);

		// public functiondecls and type decls.
		// Codegen::FunctionTree* publicFuncTree = new Codegen::FunctionTree("");

		// top level stuff
		std::vector<Expr*> topLevelExpressions;
		std::vector<NamespaceDecl*> topLevelNamespaces;

		// for typeinfo, not codegen.
		std::vector<std::tuple<std::string, fir::Type*, Codegen::TypeKind>> typeList;


		// the module-level global constructor trampoline that initialises static and global variables
		// that require init().
		// this will be called by a top-level trampoline that calls everything when all the modules are linked together
		fir::Function* globalConstructorTrampoline = 0;
	};
}




















namespace Parser
{
	const std::string& arithmeticOpToString(Codegen::CodegenInstance*, Ast::ArithmeticOp op);
	Ast::ArithmeticOp mangledStringToOperator(Codegen::CodegenInstance*, std::string op);
	std::string operatorToMangledString(Codegen::CodegenInstance*, Ast::ArithmeticOp op);

	std::string pinToString(Pin p);
}







