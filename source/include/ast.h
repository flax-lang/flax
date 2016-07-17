// ast.h
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once

#include <map>
#include <string>
#include <deque>

#include "typeinfo.h"
#include "defs.h"

namespace fir
{
	struct PHINode;
	struct StructType;
}

#define INTUNSPEC_TYPE_STRING	"int"
#define INT8_TYPE_STRING		"int8"
#define INT16_TYPE_STRING		"int16"
#define INT32_TYPE_STRING		"int32"
#define INT64_TYPE_STRING		"int64"

#define UINTUNSPEC_TYPE_STRING	"uint"
#define UINT8_TYPE_STRING		"uint8"
#define UINT16_TYPE_STRING		"uint16"
#define UINT32_TYPE_STRING		"uint32"
#define UINT64_TYPE_STRING		"uint64"

#define FLOAT32_TYPE_STRING		"float32"
#define FLOAT64_TYPE_STRING		"float64"

#define FLOAT_TYPE_STRING		"float"

#define BOOL_TYPE_STRING		"bool"
#define VOID_TYPE_STRING		"void"

#define FUNC_KEYWORD_STRING		"func"



// todo: name overhaul
// stop using std::string for names
// it's stupid and inflexible
// and *DO NOT* mangle function args during codegen to FIR
// name mangling should only be done at one location: when creating the actual function.

// note: debate moving FIR over to an identifier system like this
// FIR should do the argument type mangling, anyway

// here: store the scope and args (if applicable) of a function
// for foo::bar::qux::some_function(a: int, b: string)
// scope would contain { foo, bar, qux }
// actualname would contain { some_function }
// args would contain { int, string }


enum class IdKind
{
	Invalid,
	Name,
	Variable,
	Function,
	Method,
	Getter,
	Setter,
	Operator,
	AutoGenFunc,
	ModuleConstructor,
	Struct,
};

struct Identifier
{
	std::string name;
	std::deque<std::string> scope;
	IdKind kind = IdKind::Invalid;

	std::deque<fir::Type*> functionArguments;

	// defined in CodegenUtils.cpp
	bool operator == (const Identifier& other) const;
	bool operator != (const Identifier& other) const { return !(*this == other); }
	bool operator < (const Identifier& other) const { return this->str() < other.str(); }

	std::string str() const;
	std::string mangled() const;

	Identifier() { }
	Identifier(std::string _name, IdKind _kind) : name(_name), scope({ }), kind(_kind) { }
	Identifier(std::string _name, std::deque<std::string> _scope, IdKind _kind) : name(_name), scope(_scope), kind(_kind) { }
};








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

	typedef std::pair<fir::Value*, fir::Value*> ValPtr_t;
	enum class ResultType { Normal, BreakCodegen };
	struct Result_t
	{
		explicit Result_t(ValPtr_t vp) : result(vp), type(ResultType::Normal) { }


		Result_t(fir::Value* val, fir::Value* ptr, ResultType rt) : result(val, ptr), type(rt) { }
		Result_t(fir::Value* val, fir::Value* ptr) : result(val, ptr), type(ResultType::Normal) { }

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
		fir::Type* ftype = 0;

		ExprType() : isLiteral(true), strType(""), type(0) { }
		ExprType(std::string s) : isLiteral(true), strType(s), type(0) { }

		void operator=(std::string stryp)
		{
			this->strType = stryp;
			this->isLiteral = true;
		}

		void operator=(fir::Type* ft)
		{
			this->ftype = ft;
			this->isLiteral = true;
		}
	};



	struct Expr
	{
		explicit Expr(Parser::Pin pos) : pin(pos) { }
		virtual ~Expr() { }
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) = 0;
		virtual bool isBreaking() { return false; }

		bool didCodegen = false;
		uint64_t attribs = 0;
		Parser::Pin pin;
		ExprType type;
	};

	struct DummyExpr : Expr
	{
		explicit DummyExpr(Parser::Pin pos) : Expr(pos) { }
		~DummyExpr();
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override { return Result_t(0, 0); }
	};

	struct VarArg : Expr
	{
		~VarArg();
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override { return Result_t(0, 0); }
	};


	struct Number : Expr
	{
		~Number();
		Number(Parser::Pin pos, double val) : Expr(pos), dval(val) { this->decimal = true; }
		Number(Parser::Pin pos, int64_t val) : Expr(pos), ival(val) { this->decimal = false; }
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override;

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
		BoolVal(Parser::Pin pos, bool val) : Expr(pos), val(val) { }
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override;

		bool val = false;
	};

	struct NullVal : Expr
	{
		~NullVal();
		NullVal(Parser::Pin pos) : Expr(pos) { }
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override;
	};





	struct VarRef : Expr
	{
		~VarRef();
		VarRef(Parser::Pin pos, std::string name) : Expr(pos), name(name) { }
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override;

		std::string name;
	};



	struct VarDecl : Expr
	{
		~VarDecl();
		VarDecl(Parser::Pin pos, std::string name, bool immut) : Expr(pos), _name(name), immutable(immut)
		{
			ident.name = name;
			ident.kind = IdKind::Variable;
		}

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override;

		fir::Value* doInitialValue(Codegen::CodegenInstance* cgi, Codegen::TypePair_t* type, fir::Value* val, fir::Value* valptr, fir::Value* storage, bool shouldAddToSymtab);

		void inferType(Codegen::CodegenInstance* cgi);

		Identifier ident;
		std::string _name;

		bool immutable = false;

		bool isStatic = false;
		bool isGlobal = false;
		bool disableAutoInit = false;
		Expr* initVal = 0;
		fir::Type* inferredLType = 0;
	};

	struct BracedBlock;
	struct ComputedProperty : VarDecl
	{
		~ComputedProperty();
		ComputedProperty(Parser::Pin pos, std::string name) : VarDecl(pos, name, false) { }
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override;

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
		BinOp(Parser::Pin pos, Expr* lhs, ArithmeticOp operation, Expr* rhs) : Expr(pos), left(lhs), right(rhs), op(operation) { }
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override;

		Expr* left = 0;
		Expr* right = 0;

		ArithmeticOp op = ArithmeticOp::Invalid;
		fir::PHINode* phi = 0;
	};

	struct StructBase;
	struct FuncDecl : Expr
	{
		~FuncDecl();
		FuncDecl(Parser::Pin pos, std::string id, std::deque<VarDecl*> params, std::string ret) : Expr(pos), params(params)
		{
			this->type.strType = ret;
			this->ident.name = id;
			this->ident.kind = IdKind::Function;
		}
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override;

		Result_t generateDeclForGenericType(Codegen::CodegenInstance* cgi, std::map<std::string, fir::Type*> types);

		Parser::Pin returnTypePos;

		bool isCStyleVarArg = false;

		bool isVariadic = false;
		std::string variadicStrType;
		fir::Type* variadicType = 0;

		bool isFFI = false;
		bool isStatic = false;
		bool wasCalled = false;

		StructBase* parentClass = 0;
		FFIType ffiType = FFIType::C;

		Identifier ident;

		std::deque<VarDecl*> params;
		std::deque<std::string> genericTypes;

		fir::Type* instantiatedGenericReturnType = 0;
		std::deque<fir::Type*> instantiatedGenericTypes;
	};








	struct DeferredExpr;
	struct BracedBlock : Expr
	{
		explicit BracedBlock(Parser::Pin pos) : Expr(pos) { }
		~BracedBlock();
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override;

		std::deque<Expr*> statements;
		std::deque<DeferredExpr*> deferredStatements;
	};

	struct Func : Expr
	{
		~Func();
		Func(Parser::Pin pos, FuncDecl* funcdecl, BracedBlock* block) : Expr(pos), decl(funcdecl), block(block) { }
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override;

		FuncDecl* decl = 0;
		BracedBlock* block = 0;

		std::deque<std::deque<fir::Type*>> instantiatedGenericVersions;
	};

	struct FuncCall : Expr
	{
		~FuncCall();
		FuncCall(Parser::Pin pos, std::string target, std::deque<Expr*> args) : Expr(pos), name(target), params(args) { }
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override;

		std::string name;
		std::deque<Expr*> params;

		fir::Function* cachedGenericFuncTarget = 0;
		Codegen::Resolved_t cachedResolveTarget;
	};

	struct Return : Expr
	{
		~Return();
		Return(Parser::Pin pos, Expr* e) : Expr(pos), val(e) { }
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override;
		virtual bool isBreaking() override { return true; }

		Expr* val = 0;
		fir::Value* actualReturnValue = 0;
	};

	struct Import : Expr
	{
		~Import();
		Import(Parser::Pin pos, std::string name) : Expr(pos), module(name) { }
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override { return Result_t(0, 0); }

		std::string module;
	};

	struct ForeignFuncDecl : Expr
	{
		~ForeignFuncDecl();
		ForeignFuncDecl(Parser::Pin pos, FuncDecl* func) : Expr(pos), decl(func) { }
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override;

		FuncDecl* decl = 0;
	};

	struct DeferredExpr : Expr
	{
		DeferredExpr(Parser::Pin pos, Expr* e) : Expr(pos), expr(e) { }
		~DeferredExpr();

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override;

		Expr* expr = 0;
	};

	struct BreakableBracedBlock : Expr
	{
		explicit BreakableBracedBlock(Parser::Pin pos) : Expr(pos) { }
		~BreakableBracedBlock();
	};

	struct IfStmt : Expr
	{
		~IfStmt();
		IfStmt(Parser::Pin pos, std::deque<std::pair<Expr*, BracedBlock*>> cases, BracedBlock* ecase) : Expr(pos),
			final(ecase), cases(cases), _cases(cases) { }

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override;


		BracedBlock* final = 0;
		std::deque<std::pair<Expr*, BracedBlock*>> cases;
		std::deque<std::pair<Expr*, BracedBlock*>> _cases;	// needed to preserve stuff, since If->codegen modifies this->cases
	};

	struct WhileLoop : BreakableBracedBlock
	{
		~WhileLoop();
		WhileLoop(Parser::Pin pos, Expr* _cond, BracedBlock* _body, bool dowhile) : BreakableBracedBlock(pos),
			cond(_cond), body(_body), isDoWhileVariant(dowhile) { }

		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override;

		Expr* cond = 0;
		BracedBlock* body = 0;
		bool isDoWhileVariant = false;
	};

	struct ForLoop : BreakableBracedBlock
	{
		~ForLoop();
		ForLoop(Parser::Pin pos, VarDecl* _var, Expr* _cond, Expr* _eval) : BreakableBracedBlock(pos),
			var(_var), cond(_cond), eval(_eval) { }

		VarDecl* var = 0;
		Expr* cond = 0;
		Expr* eval = 0;
	};

	struct ForeachLoop : BreakableBracedBlock
	{

	};

	struct Break : Expr
	{
		~Break();
		explicit Break(Parser::Pin pos) : Expr(pos) { }
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override;
		virtual bool isBreaking() override { return true; }
	};

	struct Continue : Expr
	{
		~Continue();
		explicit Continue(Parser::Pin pos) : Expr(pos) { }
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override;
		virtual bool isBreaking() override { return true; }
	};

	struct UnaryOp : Expr
	{
		~UnaryOp();
		UnaryOp(Parser::Pin pos, ArithmeticOp op, Expr* expr) : Expr(pos), op(op), expr(expr) { }
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override;

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
		OpOverload(Parser::Pin pos, ArithmeticOp op) : Expr(pos), op(op) { }
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override;

		ArithmeticOp op = ArithmeticOp::Invalid;
		OperatorKind kind = OperatorKind::Invalid;

		Func* func = 0;
		fir::Function* lfunc = 0;
	};


	struct SubscriptOpOverload : Expr
	{
		~SubscriptOpOverload();
		SubscriptOpOverload(Parser::Pin pos) : Expr(pos) { }
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override;

		std::string setterArgName;

		FuncDecl* decl = 0;

		BracedBlock* getterBody = 0;
		BracedBlock* setterBody = 0;

		fir::Function* getterFunc = 0;
		fir::Function* setterFunc = 0;
	};

	struct AssignOpOverload : Expr
	{
		~AssignOpOverload();
		AssignOpOverload(Parser::Pin pos, ArithmeticOp o) : Expr(pos), op(o) { }
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override;

		Func* func = 0;
		ArithmeticOp op;
		fir::Function* lfunc = 0;
	};


	struct StructBase : Expr
	{
		virtual ~StructBase();
		StructBase(Parser::Pin pos, std::string name) : Expr(pos)
		{
			this->ident.name = name;
			this->ident.kind = IdKind::Struct;
		}
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override = 0;
		virtual fir::Type* createType(Codegen::CodegenInstance* cgi, std::map<std::string, fir::Type*> instantiatedGenericTypes = { }) = 0;

		bool didCreateType = false;
		fir::StructType* createdType = 0;

		Identifier ident;

		std::deque<VarDecl*> members;
		std::map<std::string, int> nameMap;
		std::deque<fir::Function*> initFuncs;

		std::deque<std::pair<StructBase*, fir::Type*>> nestedTypes;
	};

	struct ExtensionDef;
	struct ClassDef : StructBase
	{
		~ClassDef();
		ClassDef(Parser::Pin pos, std::string name) : StructBase(pos, name) { }
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override;
		virtual fir::Type* createType(Codegen::CodegenInstance* cgi, std::map<std::string, fir::Type*> instantiatedGenericTypes = { }) override;

		std::deque<Func*> funcs;
		std::deque<fir::Function*> lfuncs;
		std::deque<ComputedProperty*> cprops;
		std::deque<std::string> protocolstrs;

		std::map<Func*, fir::Function*> functionMap;

		std::deque<SubscriptOpOverload*> subscriptOverloads;
		std::deque<AssignOpOverload*> assignmentOverloads;
	};


	struct ExtensionDef : StructBase
	{
		~ExtensionDef();
		ExtensionDef(Parser::Pin pos, std::string name) : StructBase(pos, name) { }
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override;
		virtual fir::Type* createType(Codegen::CodegenInstance* cgi, std::map<std::string, fir::Type*> instantiatedGenericTypes = { }) override;

		bool isDuplicate = false;

		std::deque<Func*> funcs;
		std::deque<fir::Function*> lfuncs;
		std::deque<std::string> protocolstrs;
		std::deque<ComputedProperty*> cprops;
		std::map<Func*, fir::Function*> functionMap;
		std::deque<AssignOpOverload*> assignmentOverloads;
		std::deque<SubscriptOpOverload*> subscriptOverloads;
	};




	struct StructDef : StructBase
	{
		~StructDef();
		StructDef(Parser::Pin pos, std::string name) : StructBase(pos, name) { }
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override;
		virtual fir::Type* createType(Codegen::CodegenInstance* cgi, std::map<std::string, fir::Type*> instantiatedGenericTypes = { }) override;

		bool packed = false;
	};

	struct EnumDef : ClassDef
	{
		~EnumDef();
		EnumDef(Parser::Pin pos, std::string name) : ClassDef(pos, name) { }
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override;
		virtual fir::Type* createType(Codegen::CodegenInstance* cgi, std::map<std::string, fir::Type*> instantiatedGenericTypes = { }) override;

		std::deque<std::pair<std::string, Expr*>> cases;
		bool isStrong = false;
	};

	struct Tuple : StructBase
	{
		~Tuple();
		Tuple(Parser::Pin pos, std::vector<Expr*> _values) : StructBase(pos, ""), values(_values) { }
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override;
		virtual fir::Type* createType(Codegen::CodegenInstance* cgi, std::map<std::string, fir::Type*> instantiatedGenericTypes = { }) override;
		fir::StructType* getType(Codegen::CodegenInstance* cgi);

		std::vector<Expr*> values;
		std::vector<fir::Type*> ltypes;

		fir::StructType* cachedLlvmType = 0;
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
		MemberAccess(Parser::Pin pos, Expr* _left, Expr* _right) : Expr(pos), left(_left), right(_right) { }
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override;

		bool disableStaticChecking = false;
		Result_t cachedCodegenResult = Result_t(0, 0);
		Expr* left = 0;
		Expr* right = 0;

		MAType matype = MAType::Invalid;
	};




	struct NamespaceDecl : Expr
	{
		~NamespaceDecl();
		NamespaceDecl(Parser::Pin pos, std::string _name, BracedBlock* inside) : Expr(pos), innards(inside), name(_name)
		{ }
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override { return Result_t(0, 0); }

		void codegenPass(Codegen::CodegenInstance* cgi, int pass);

		std::deque<NamespaceDecl*> namespaces;
		BracedBlock* innards = 0;
		std::string name;
	};

	struct ArrayIndex : Expr
	{
		~ArrayIndex();
		ArrayIndex(Parser::Pin pos, Expr* v, Expr* index) : Expr(pos), arr(v), index(index) { }
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override;

		Expr* arr = 0;
		Expr* index = 0;
	};

	struct StringLiteral : Expr
	{
		~StringLiteral();
		StringLiteral(Parser::Pin pos, std::string str) : Expr(pos), str(str) { }
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override;

		bool isRaw = false;
		std::string str;
	};

	struct ArrayLiteral : Expr
	{
		~ArrayLiteral();
		ArrayLiteral(Parser::Pin pos, std::deque<Expr*> values) : Expr(pos), values(values) { }
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override;

		std::deque<Expr*> values;
	};

	struct TypeAlias : StructBase
	{
		~TypeAlias();
		TypeAlias(Parser::Pin pos, std::string _alias, std::string _origType) : StructBase(pos, _alias), origType(_origType) { }
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override;
		virtual fir::Type* createType(Codegen::CodegenInstance* cgi, std::map<std::string, fir::Type*> instantiatedGenericTypes = { }) override;

		bool isStrong = false;
		std::string origType;
	};

	struct Alloc : Expr
	{
		~Alloc();
		explicit Alloc(Parser::Pin pos) : Expr(pos) { }
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override;

		std::deque<Expr*> counts;
		std::deque<Expr*> params;
	};

	struct Dealloc : Expr
	{
		~Dealloc();
		Dealloc(Parser::Pin pos, Expr* _expr) : Expr(pos), expr(_expr) { }
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override;

		Expr* expr = 0;
	};

	struct Typeof : Expr
	{
		~Typeof();
		Typeof(Parser::Pin pos, Expr* _inside) : Expr(pos), inside(_inside) { }
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override;

		Expr* inside = 0;
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
		PostfixUnaryOp(Parser::Pin pos, Expr* e, Kind k) : Expr(pos), kind(k), expr(e) { }
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override;

		Kind kind;
		Expr* expr = 0;
		std::deque<Expr*> args;
	};

	struct Root : Expr
	{
		Root() : Expr(Parser::Pin()) { }
		~Root();
		virtual Result_t codegen(Codegen::CodegenInstance* cgi, fir::Value* extra = 0) override;

		Codegen::FunctionTree* rootFuncStack = new Codegen::FunctionTree("__#root");

		// public functiondecls and type decls.
		Codegen::FunctionTree* publicFuncTree = new Codegen::FunctionTree("");

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









