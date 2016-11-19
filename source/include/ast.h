// ast.h
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once

#include <map>
#include <string>
#include <deque>

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
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) = 0;
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, bool allowFail = false, fir::Value* extra = 0) = 0;

		virtual bool isBreaking() { return false; }

		bool didCodegen = false;
		uint64_t attribs = 0;
		Parser::Pin pin;

		// ExprType type;
		pts::Type* ptype = 0;
	};

	struct DummyExpr : Expr
	{
		explicit DummyExpr(const Parser::Pin& pos) : Expr(pos) { }
		~DummyExpr();

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override { return Result_t(0, 0); }
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, bool allowFail = false, fir::Value* extra = 0) override;
	};


	struct Number : Expr
	{
		~Number();
		Number(const Parser::Pin& pos, double val) : Expr(pos), dval(val) { this->decimal = true; }
		Number(const Parser::Pin& pos, int64_t val) : Expr(pos), ival(val) { this->decimal = false; }

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override;
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, bool allowFail = false, fir::Value* extra = 0) override;

		bool needUnsigned = false;
		bool decimal = false;
		fir::Type* properLlvmType = 0;

		union
		{
			int64_t ival;
			double dval;
		};
	};

	struct BoolVal : Expr
	{
		~BoolVal();
		BoolVal(const Parser::Pin& pos, bool val) : Expr(pos), val(val) { }

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override;
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, bool allowFail = false, fir::Value* extra = 0) override;

		bool val = false;
	};

	struct NullVal : Expr
	{
		~NullVal();
		NullVal(const Parser::Pin& pos) : Expr(pos) { }

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override;
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, bool allowFail = false, fir::Value* extra = 0) override;
	};





	struct VarRef : Expr
	{
		~VarRef();
		VarRef(const Parser::Pin& pos, std::string name) : Expr(pos), name(name) { }

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override;
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, bool allowFail = false, fir::Value* extra = 0) override;

		std::string name;
	};



	struct VarDecl : Expr
	{
		~VarDecl();
		VarDecl(const Parser::Pin& pos, std::string name, bool immut) : Expr(pos), immutable(immut)
		{
			ident.name = name;
			ident.kind = IdKind::Variable;
		}

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override;
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, bool allowFail = false, fir::Value* extra = 0) override;

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

	struct BracedBlock;
	struct ComputedProperty : VarDecl
	{
		~ComputedProperty();
		ComputedProperty(const Parser::Pin& pos, std::string name) : VarDecl(pos, name, false) { }

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override;
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, bool allowFail = false, fir::Value* extra = 0) override;

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
		~BinOp();
		BinOp(const Parser::Pin& pos, Expr* lhs, ArithmeticOp operation, Expr* rhs) : Expr(pos), left(lhs), right(rhs), op(operation) { }

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override;
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, bool allowFail = false, fir::Value* extra = 0) override;

		Expr* left = 0;
		Expr* right = 0;

		ArithmeticOp op = ArithmeticOp::Invalid;
	};

	struct StructBase;
	struct FuncDecl : Expr
	{
		~FuncDecl();
		FuncDecl(const Parser::Pin& pos, std::string id, std::deque<VarDecl*> params, pts::Type* ret) : Expr(pos), params(params)
		{
			this->ident.name = id;
			this->ident.kind = IdKind::Function;

			this->ptype = ret;
		}

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override;
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, bool allowFail = false, fir::Value* extra = 0) override;

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

		std::deque<VarDecl*> params;
		std::map<std::string, TypeConstraints_t> genericTypes;


	};








	struct DeferredExpr;
	struct BracedBlock : Expr
	{
		explicit BracedBlock(const Parser::Pin& pos) : Expr(pos) { }
		~BracedBlock();

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override;
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, bool allowFail = false, fir::Value* extra = 0) override;

		std::deque<Expr*> statements;
		std::deque<DeferredExpr*> deferredStatements;
	};

	struct Func : Expr
	{
		~Func();
		Func(const Parser::Pin& pos, FuncDecl* funcdecl, BracedBlock* block) : Expr(pos), decl(funcdecl), block(block) { }

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override;
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, bool allowFail = false, fir::Value* extra = 0) override;

		FuncDecl* decl = 0;
		BracedBlock* block = 0;
	};

	struct FuncCall : Expr
	{
		~FuncCall();
		FuncCall(const Parser::Pin& pos, std::string target, std::deque<Expr*> args) : Expr(pos), name(target), params(args) { }

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override;
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, bool allowFail = false, fir::Value* extra = 0) override;

		std::string name;
		std::deque<Expr*> params;

		Codegen::Resolved_t cachedResolveTarget;
	};

	struct Return : Expr
	{
		~Return();
		Return(const Parser::Pin& pos, Expr* e) : Expr(pos), val(e) { }

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override;
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, bool allowFail = false, fir::Value* extra = 0) override;
		virtual bool isBreaking() override { return true; }

		Expr* val = 0;
		fir::Value* actualReturnValue = 0;
	};

	struct Import : Expr
	{
		~Import();
		Import(const Parser::Pin& pos, std::string name) : Expr(pos), module(name) { }

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override { return Result_t(0, 0); }
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, bool allowFail = false, fir::Value* extra = 0) override { return 0; };

		std::string module;
	};

	struct ForeignFuncDecl : Expr
	{
		~ForeignFuncDecl();
		ForeignFuncDecl(const Parser::Pin& pos, FuncDecl* func) : Expr(pos), decl(func) { }

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override;
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, bool allowFail = false, fir::Value* extra = 0) override;

		FuncDecl* decl = 0;
	};

	struct DeferredExpr : Expr
	{
		DeferredExpr(const Parser::Pin& pos, Expr* e) : Expr(pos), expr(e) { }
		~DeferredExpr();

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override;
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, bool allowFail = false, fir::Value* extra = 0) override;

		Expr* expr = 0;
	};

	struct BreakableBracedBlock : Expr
	{
		explicit BreakableBracedBlock(const Parser::Pin& pos, BracedBlock* _body) : Expr(pos), body(_body) { }
		~BreakableBracedBlock();

		BracedBlock* body = 0;
	};

	struct IfStmt : Expr
	{
		~IfStmt();
		IfStmt(const Parser::Pin& pos, std::deque<std::pair<Expr*, BracedBlock*>> cases, BracedBlock* ecase) : Expr(pos),
			final(ecase), cases(cases), _cases(cases) { }

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override;
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, bool allowFail = false, fir::Value* extra = 0) override;


		BracedBlock* final = 0;
		std::deque<std::pair<Expr*, BracedBlock*>> cases;
		std::deque<std::pair<Expr*, BracedBlock*>> _cases;	// needed to preserve stuff, since If->codegen modifies this->cases
	};

	struct WhileLoop : BreakableBracedBlock
	{
		~WhileLoop();
		WhileLoop(const Parser::Pin& pos, Expr* _cond, BracedBlock* _body, bool dowhile) : BreakableBracedBlock(pos, _body),
			cond(_cond), isDoWhileVariant(dowhile) { }

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override;
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, bool allowFail = false, fir::Value* extra = 0) override;

		Expr* cond = 0;
		bool isDoWhileVariant = false;
	};

	#if 0
	struct ForLoop : BreakableBracedBlock
	{
		~ForLoop();
		ForLoop(const Parser::Pin& pos, VarDecl* _var, Expr* _cond, Expr* _eval) : BreakableBracedBlock(pos),
			var(_var), cond(_cond), eval(_eval) { }

		VarDecl* var = 0;
		Expr* cond = 0;
		Expr* eval = 0;
	};

	struct ForeachLoop : BreakableBracedBlock
	{

	};
	#endif

	struct Break : Expr
	{
		~Break();
		explicit Break(const Parser::Pin& pos) : Expr(pos) { }

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override;
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, bool allowFail = false, fir::Value* extra = 0) override;
		virtual bool isBreaking() override { return true; }
	};

	struct Continue : Expr
	{
		~Continue();
		explicit Continue(const Parser::Pin& pos) : Expr(pos) { }

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override;
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, bool allowFail = false, fir::Value* extra = 0) override;
		virtual bool isBreaking() override { return true; }
	};

	struct UnaryOp : Expr
	{
		~UnaryOp();
		UnaryOp(const Parser::Pin& pos, ArithmeticOp op, Expr* expr) : Expr(pos), op(op), expr(expr) { }

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override;
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, bool allowFail = false, fir::Value* extra = 0) override;

		ArithmeticOp op;
		Expr* expr = 0;
	};

	// fuck
	struct OpOverload : Expr
	{
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

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override;
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, bool allowFail = false, fir::Value* extra = 0) override;

		Result_t codegen(Codegen::CodegenInstance* cgi, std::deque<fir::Type*> args);

		ArithmeticOp op = ArithmeticOp::Invalid;
		OperatorKind kind = OperatorKind::Invalid;

		Func* func = 0;
		fir::Function* lfunc = 0;
	};


	struct SubscriptOpOverload : Expr
	{
		~SubscriptOpOverload();
		SubscriptOpOverload(const Parser::Pin& pos) : Expr(pos) { }

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override;
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, bool allowFail = false, fir::Value* extra = 0) override;

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
		~AssignOpOverload();
		AssignOpOverload(const Parser::Pin& pos, ArithmeticOp o) : Expr(pos), op(o) { }

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override;
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, bool allowFail = false, fir::Value* extra = 0) override;

		Func* func = 0;
		ArithmeticOp op;
		fir::Function* lfunc = 0;
	};


	struct StructBase : Expr
	{
		virtual ~StructBase();
		StructBase(const Parser::Pin& pos, std::string name) : Expr(pos)
		{
			this->ident.name = name;
			this->ident.kind = IdKind::Struct;
		}

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override = 0;
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, bool allowFail = false, fir::Value* extra = 0) override = 0;
		virtual fir::Type* createType(Codegen::CodegenInstance* cgi) = 0;

		bool didCreateType = false;
		fir::Type* createdType = 0;

		Identifier ident;

		std::deque<VarDecl*> members;
		std::deque<fir::Function*> initFuncs;

		fir::Function* defaultInitialiser;

		std::deque<std::pair<StructBase*, fir::Type*>> nestedTypes;
	};

	struct ClassDef : StructBase
	{
		~ClassDef();
		ClassDef(const Parser::Pin& pos, std::string name) : StructBase(pos, name) { }

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override;
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, bool allowFail = false, fir::Value* extra = 0) override;
		virtual fir::Type* createType(Codegen::CodegenInstance* cgi) override;

		std::deque<Func*> funcs;
		std::deque<fir::Function*> lfuncs;
		std::deque<ComputedProperty*> cprops;
		std::deque<std::string> protocolstrs;
		std::deque<OpOverload*> operatorOverloads;
		std::deque<AssignOpOverload*> assignmentOverloads;
		std::deque<SubscriptOpOverload*> subscriptOverloads;
		std::unordered_map<Func*, fir::Function*> functionMap;

		std::deque<ProtocolDef*> conformedProtocols;
	};


	struct Root;
	struct ExtensionDef : ClassDef
	{
		~ExtensionDef();
		ExtensionDef(const Parser::Pin& pos, std::string name) : ClassDef(pos, name) { }

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override;
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, bool allowFail = false, fir::Value* extra = 0) override;
		virtual fir::Type* createType(Codegen::CodegenInstance* cgi) override;

		bool isDuplicate = false;
		Root* parentRoot = 0;
	};

	struct ProtocolDef : Expr
	{
		~ProtocolDef();
		ProtocolDef(const Parser::Pin& pos, std::string name) : Expr(pos)
		{
			this->ident.name = name;
			this->ident.kind = IdKind::Struct;
		}


		fir::Type* createType(Codegen::CodegenInstance* cgi);
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, bool allowFail = false, fir::Value* extra = 0) override;
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override;

		bool checkTypeConformity(Codegen::CodegenInstance* cgi, fir::Type* type);
		void assertTypeConformity(Codegen::CodegenInstance* cgi, fir::Type* type);

		Identifier ident;

		std::deque<std::string> protocolstrs;

		std::deque<Func*> funcs;
		std::deque<OpOverload*> operatorOverloads;
		std::deque<AssignOpOverload*> assignmentOverloads;
		std::deque<SubscriptOpOverload*> subscriptOverloads;
	};



	struct StructDef : StructBase
	{
		~StructDef();
		StructDef(const Parser::Pin& pos, std::string name) : StructBase(pos, name) { }

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override;
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, bool allowFail = false, fir::Value* extra = 0) override;
		virtual fir::Type* createType(Codegen::CodegenInstance* cgi) override;

		bool packed = false;
	};

	struct EnumDef : StructBase
	{
		~EnumDef();
		EnumDef(const Parser::Pin& pos, std::string name) : StructBase(pos, name) { }

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override;
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, bool allowFail = false, fir::Value* extra = 0) override;
		virtual fir::Type* createType(Codegen::CodegenInstance* cgi) override;

		std::deque<std::pair<std::string, Expr*>> cases;
		bool isStrong = false;
	};

	struct Tuple : StructBase
	{
		~Tuple();
		Tuple(const Parser::Pin& pos, std::vector<Expr*> _values) : StructBase(pos, ""), values(_values) { }

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override;
		virtual fir::TupleType* getType(Codegen::CodegenInstance* cgi, bool allowFail = false, fir::Value* extra = 0) override;
		virtual fir::Type* createType(Codegen::CodegenInstance* cgi) override;

		std::vector<Expr*> values;
		std::vector<fir::Type*> ltypes;

		fir::TupleType* createdType = 0;
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
		MemberAccess(const Parser::Pin& pos, Expr* _left, Expr* _right) : Expr(pos), left(_left), right(_right) { }

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override;
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, bool allowFail = false, fir::Value* extra = 0) override;

		bool disableStaticChecking = false;
		Result_t cachedCodegenResult = Result_t(0, 0);
		Expr* left = 0;
		Expr* right = 0;

		MAType matype = MAType::Invalid;
	};




	struct NamespaceDecl : Expr
	{
		~NamespaceDecl();
		NamespaceDecl(const Parser::Pin& pos, std::string _name, BracedBlock* inside) : Expr(pos), innards(inside), name(_name) { }

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override { return Result_t(0, 0); }
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, bool allowFail = false, fir::Value* extra = 0) override { return 0; };

		void codegenPass(Codegen::CodegenInstance* cgi, int pass);

		std::deque<NamespaceDecl*> namespaces;
		BracedBlock* innards = 0;
		std::string name;
	};

	struct ArrayIndex : Expr
	{
		~ArrayIndex();
		ArrayIndex(const Parser::Pin& pos, Expr* v, Expr* index) : Expr(pos), arr(v), index(index) { }

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override;
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, bool allowFail = false, fir::Value* extra = 0) override;

		Expr* arr = 0;
		Expr* index = 0;
	};

	struct StringLiteral : Expr
	{
		~StringLiteral();
		StringLiteral(const Parser::Pin& pos, std::string str) : Expr(pos), str(str) { }

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override;
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, bool allowFail = false, fir::Value* extra = 0) override;

		bool isRaw = false;
		std::string str;
	};

	struct ArrayLiteral : Expr
	{
		~ArrayLiteral();
		ArrayLiteral(const Parser::Pin& pos, std::deque<Expr*> values) : Expr(pos), values(values) { }

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override;
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, bool allowFail = false, fir::Value* extra = 0) override;

		std::deque<Expr*> values;
	};

	struct TypeAlias : StructBase
	{
		~TypeAlias();
		TypeAlias(const Parser::Pin& pos, std::string _alias, pts::Type* _origType) : StructBase(pos, _alias), origType(_origType) { }

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override;
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, bool allowFail = false, fir::Value* extra = 0) override;
		virtual fir::Type* createType(Codegen::CodegenInstance* cgi) override;

		bool isStrong = false;
		pts::Type* origType;
	};

	struct Alloc : Expr
	{
		~Alloc();
		explicit Alloc(const Parser::Pin& pos) : Expr(pos) { }

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override;
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, bool allowFail = false, fir::Value* extra = 0) override;

		std::deque<Expr*> counts;
		std::deque<Expr*> params;
	};

	struct Dealloc : Expr
	{
		~Dealloc();
		Dealloc(const Parser::Pin& pos, Expr* _expr) : Expr(pos), expr(_expr) { }

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override;
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, bool allowFail = false, fir::Value* extra = 0) override;

		Expr* expr = 0;
	};

	struct Typeof : Expr
	{
		~Typeof();
		Typeof(const Parser::Pin& pos, Expr* _inside) : Expr(pos), inside(_inside) { }

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override;
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, bool allowFail = false, fir::Value* extra = 0) override;

		Expr* inside = 0;
	};

	struct PostfixUnaryOp : Expr
	{
		enum class Kind
		{
			Invalid,
			Increment,
			Decrement
		};

		~PostfixUnaryOp();
		PostfixUnaryOp(const Parser::Pin& pos, Expr* e, Kind k) : Expr(pos), kind(k), expr(e) { }

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override;
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, bool allowFail = false, fir::Value* extra = 0) override;

		Kind kind;
		Expr* expr = 0;
		std::deque<Expr*> args;
	};

	struct Root : Expr
	{
		Root() : Expr(Parser::Pin()) { }
		~Root();

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override;
		virtual fir::Type* getType(Codegen::CodegenInstance* cgi, bool allowFail = false, fir::Value* extra = 0) override { return 0; };

		Codegen::FunctionTree* rootFuncStack = new Codegen::FunctionTree("__#root");

		// public functiondecls and type decls.
		// Codegen::FunctionTree* publicFuncTree = new Codegen::FunctionTree("");

		// top level stuff
		std::deque<Expr*> topLevelExpressions;
		std::deque<NamespaceDecl*> topLevelNamespaces;

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
	std::string arithmeticOpToString(Codegen::CodegenInstance*, Ast::ArithmeticOp op);
	Ast::ArithmeticOp mangledStringToOperator(Codegen::CodegenInstance*, std::string op);
	std::string operatorToMangledString(Codegen::CodegenInstance*, Ast::ArithmeticOp op);
}







