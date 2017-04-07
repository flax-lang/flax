// cst.h
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once
#include <defs.h>

#include <ir/type.h>
#include <ir/irbuilder.h>

#include <set>
#include <map>
#include <unordered_map>

#include <experimental/optional>

namespace Cst { struct DefnType; }
namespace std { template <typename T> using optional = std::experimental::optional<T>; }


namespace Cst
{
	enum class AccessSpecifier
	{
		Invalid,
		Private,
		Protected,
		Public,
	};




	struct Unit
	{
		virtual ~Unit() { }
		explicit Unit(Parser::Pin p) : position(p) { }

		Parser::Pin position;
		fir::Type* resultType = 0;
	};

	struct BinaryOp : Unit
	{
		~BinaryOp();
		explicit BinaryOp(Parser::Pin p) : Unit(p) { }

		Unit* lhs = 0;
		Unit* rhs = 0;

		Ast::ArithmeticOp operation = Ast::ArithmeticOp::Invalid;
	};

	struct UnaryOp : Unit
	{
		~UnaryOp();
		explicit UnaryOp(Parser::Pin p) : Unit(p) { }

		Unit* expr = 0;
		Ast::ArithmeticOp operation = Ast::ArithmeticOp::Invalid;
	};

	struct SubscriptOp : Unit
	{
		~SubscriptOp();
		explicit SubscriptOp(Parser::Pin p) : Unit(p) { }

		Unit* subscriptee = 0;
		Unit* index = 0;
	};

	struct DotOp : Unit
	{
		~DotOp();
		explicit DotOp(Parser::Pin p) : Unit(p) { }

		Unit* lhs = 0;
		Unit* rhs = 0;
	};

	struct SliceOp : Unit
	{
		~SliceOp();
		explicit SliceOp(Parser::Pin p) : Unit(p) { }

		Unit* array = 0;
		Unit* start = 0;
		Unit* end = 0;
	};



	struct LitBool : Unit
	{
		~LitBool();
		explicit LitBool(Parser::Pin p) : Unit(p) { }

		bool value = false;
	};

	struct LitInteger : Unit
	{
		~LitInteger();
		explicit LitInteger(Parser::Pin p) : Unit(p) { }

		uint64_t value = 0;
		bool isSigned = false;
	};

	struct LitDecimal : Unit
	{
		~LitDecimal();
		explicit LitDecimal(Parser::Pin p) : Unit(p) { }

		long double value = 0.0;
	};

	struct LitString : Unit
	{
		~LitString();
		explicit LitString(Parser::Pin p) : Unit(p) { }

		std::string str;
	};

	struct LitNull : Unit
	{
		~LitNull();
		explicit LitNull(Parser::Pin p) : Unit(p) { }
	};

	struct LitTuple : Unit
	{
		~LitTuple();
		explicit LitTuple(Parser::Pin p) : Unit(p) { }

		std::vector<Unit*> values;
	};

	struct LitArray : Unit
	{
		~LitArray();
		explicit LitArray(Parser::Pin p) : Unit(p) { }

		std::vector<Unit*> values;
	};



	struct ExprRange : Unit
	{
		~ExprRange();
		explicit ExprRange(Parser::Pin p) : Unit(p) { }

		Unit* start = 0;
		Unit* end = 0;
	};

	struct ExprVariable : Unit
	{
		~ExprVariable();
		explicit ExprVariable(Parser::Pin p) : Unit(p) { }

		std::string name;
	};

	struct ExprTypeof : Unit
	{
		~ExprTypeof();
		explicit ExprTypeof(Parser::Pin p) : Unit(p) { }

		Unit* expr = 0;
	};

	struct ExprTypeid : Unit
	{
		~ExprTypeid();
		explicit ExprTypeid(Parser::Pin p) : Unit(p) { }

		Unit* expr = 0;
	};

	struct ExprSizeof : Unit
	{
		~ExprSizeof();
		explicit ExprSizeof(Parser::Pin p) : Unit(p) { }

		Unit* expr = 0;
	};

	struct ExprFuncCall : Unit
	{
		~ExprFuncCall();
		explicit ExprFuncCall(Parser::Pin p) : Unit(p) { }

		std::string name = 0;
		std::vector<Unit*> arguments;
	};



	struct DeclVariable : Unit
	{
		~DeclVariable();
		explicit DeclVariable(Parser::Pin p) : Unit(p) { }

		Identifier ident;
		bool isImmutable = false;
		AccessSpecifier access = AccessSpecifier::Invalid;

		Unit* initialiser = 0;
	};

	struct DeclTupleDecomp : Unit
	{
		struct Mapping
		{
			bool isRecursive = false;

			Parser::Pin pos;
			std::string name;
			std::vector<Mapping> inners;
		};

		~DeclTupleDecomp();
		explicit DeclTupleDecomp(Parser::Pin p) : Unit(p) { }

		bool isImmutable = false;
		Mapping mapping = { };
		Unit* tuple = 0;
	};

	struct DeclArrayDecomp : Unit
	{
		~DeclArrayDecomp();
		explicit DeclArrayDecomp(Parser::Pin p) : Unit(p) { }

		bool isImmutable = false;
		std::unordered_map<size_t, std::pair<std::string, Parser::Pin>> mapping;
		Unit* array = 0;
	};

	struct Block;
	struct DeclFunction : Unit
	{
		~DeclFunction();
		explicit DeclFunction(Parser::Pin p) : Unit(p) { }

		enum class VariadicType
		{
			Invalid,
			None,
			CStyle,
			Normal
		};

		Identifier ident;
		Block* functionBody = 0;
		fir::Type* returnType = 0;
		std::vector<DeclVariable*> parameters;
		AccessSpecifier access = AccessSpecifier::Invalid;
	};



	struct Block : Unit
	{
		~Block();
		explicit Block(Parser::Pin p) : Unit(p) { }

		std::vector<Unit*> things;
		std::vector<Unit*> deferredThings;
	};

	struct DefnType : Unit
	{
		~DefnType();
		explicit DefnType(Parser::Pin p) : Unit(p) { }
	};

	struct DefnStruct : DefnType
	{
		~DefnStruct();
		explicit DefnStruct(Parser::Pin p) : DefnType(p) { }

		Identifier ident;
		bool isPacked = false;
		std::vector<DeclVariable*> members;
		std::vector<DeclFunction*> initialisers;
	};

	struct _Property
	{
		std::string name;
		Block* getter = 0;
		Block* setter = 0;

		std::string setterName;
	};

	struct _SubscriptOverload
	{
		Block* getter = 0;
		Block* setter = 0;

		std::string setterName;
	};

	struct DefnClass : DefnType
	{
		~DefnClass();
		explicit DefnClass(Parser::Pin p) : DefnType(p) { }

		Identifier ident;
		std::vector<DefnType*> nestedTypes;
		std::vector<_Property> properties;
		std::vector<DeclVariable*> members;
		std::vector<DeclFunction*> initialisers;

		std::vector<DeclFunction*> assignOverloads;
		std::vector<_SubscriptOverload> subscriptOverloads;

		std::unordered_map<Ast::ArithmeticOp, std::vector<DeclFunction*>> opOverloads;

		// bool indicates whether the function is static or not
		std::vector<std::pair<DeclFunction*, bool>> methods;

		std::vector<std::string> conformedProtocols;
	};

	struct DefnEnum : DefnType
	{
		~DefnEnum();
		explicit DefnEnum(Parser::Pin p) : DefnType(p) { }

		Identifier ident;
		bool isWeak = false;
		std::vector<std::pair<std::string, Unit*>> cases;
	};

	struct DefnExtension : DefnType
	{
		~DefnExtension();
		explicit DefnExtension(Parser::Pin p) : DefnType(p) { }

		Identifier ident;
		std::vector<_Property> properties;
		std::vector<DefnType*> nestedTypes;
		std::vector<DeclFunction*> initialisers;

		std::vector<DeclFunction*> assignOverloads;
		std::vector<_SubscriptOverload> subscriptOverloads;

		std::unordered_map<Ast::ArithmeticOp, std::vector<DeclFunction*>> opOverloads;

		// bool indicates whether the function is static or not
		std::vector<std::pair<DeclFunction*, bool>> methods;
	};

	struct DefnProtocol : Unit
	{
		~DefnProtocol();
		explicit DefnProtocol(Parser::Pin p) : Unit(p) { }

		Identifier ident;
		std::vector<DeclVariable*> members;
		std::vector<DeclFunction*> initialisers;

		std::vector<DeclFunction*> assignOverloads;
		std::vector<_SubscriptOverload> subscriptOverloads;
		std::unordered_map<Ast::ArithmeticOp, std::vector<DeclFunction*>> opOverloads;

		// bool indicates whether the function is static or not
		std::vector<std::pair<DeclFunction*, bool>> methods;

		std::vector<std::string> conformedProtocols;
	};



	struct StmtIf : Unit
	{
		~StmtIf();
		explicit StmtIf(Parser::Pin p) : Unit(p) { }

		struct Case
		{
			Unit* initialiser = 0;
			Unit* condition = 0;
			Block* body = 0;
		};

		Block* final = 0;
		std::vector<Case> cases;
	};

	struct StmtAlloc : Unit
	{
		~StmtAlloc();
		explicit StmtAlloc(Parser::Pin p) : Unit(p) { }

		fir::Type* baseType = 0;
		std::vector<Unit*> counts;
		std::vector<Unit*> arguments;
	};

	struct StmtDealloc : Unit
	{
		~StmtDealloc();
		explicit StmtDealloc(Parser::Pin p) : Unit(p) { }

		Unit* expr = 0;
	};

	struct StmtBreak : Unit
	{
		~StmtBreak();
		explicit StmtBreak(Parser::Pin p) : Unit(p) { }
	};

	struct StmtContinue : Unit
	{
		~StmtContinue();
		explicit StmtContinue(Parser::Pin p) : Unit(p) { }
	};

	struct StmtReturn : Unit
	{
		~StmtReturn();
		explicit StmtReturn(Parser::Pin p) : Unit(p) { }

		Unit* expr = 0;
	};




	struct LoopWhile : Unit
	{
		~LoopWhile();
		explicit LoopWhile(Parser::Pin p) : Unit(p) { }

		enum class Variant
		{
			Invalid,
			LoopForever,
			DoWhile,
			Do
		};

		Variant kind;
		Unit* condition = 0;
	};

	struct LoopFor : Unit
	{
		~LoopFor();
		explicit LoopFor(Parser::Pin p) : Unit(p) { }

		Unit* condition = 0;
		Unit* initialiser = 0;
		std::vector<Unit*> incrs;
	};

	struct LoopForIn : Unit
	{
		~LoopForIn();
		explicit LoopForIn(Parser::Pin p) : Unit(p) { }

		std::string indexName;
		Unit* variable = 0;
		Unit* loopee = 0;
	};




	struct Tree
	{
		std::string name;

		std::multimap<std::string, DeclFunction*> functions;
		std::multimap<std::string, DeclVariable*> variables;
		std::multimap<Ast::ArithmeticOp, DeclFunction*> opOverloads;

		std::unordered_map<std::string, DefnEnum*> enums;
		std::unordered_map<std::string, DefnClass*> classes;
		std::unordered_map<std::string, DefnStruct*> structs;
		std::unordered_map<std::string, DefnProtocol*> protocols;
		std::multimap<std::string, DefnExtension*> extensions;

		// pointers to refcounted values. Tree <-> Block is *roughly* 1-to-1, so function bodies, loop bodies, etc. will
		// each have their own tree -- and hence their own set of functions, types, refcounted things, etc.

		// since we have a parentTree pointer, we can easily iterate upwards to find what we need without keeping
		// track of multiple stacks for each specific kind of thing.
		std::vector<fir::Value*> refcountedPointers;

		std::set<Identifier> _functionset;

		Tree* parentTree = 0;
		std::unordered_map<std::string, Tree*> subtrees;
	};

	struct StateWrapper
	{
		Tree* rootTree = 0;
		fir::Module* module = 0;
		fir::IRBuilder irb = fir::IRBuilder(fir::getDefaultFTContext());

		Tree* currentTree = 0;

		// pushes and pops trees
		Tree* getCurrentTree();
		Tree* pushTree(std::string name);
		Tree* popTree();

		std::optional<std::pair<DefnType*, fir::Type*>> findTypeByName(std::string name);
		void addNewType(DefnType* t, fir::Type* ft);


	};
}
























