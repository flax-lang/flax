// defs.h
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>

#include <string>
#include <map>
#include <unordered_map>
#include <deque>

#include <sys/types.h>

#define TAB_WIDTH	4

#include "iceassert.h"

// forward declarations.
namespace fir
{
	struct Value;
	struct Type;
	struct Function;
	struct IRBlock;
}

namespace Ast
{
	struct Expr;
	struct Func;
	struct VarDecl;
	struct FuncDecl;
	struct OpOverload;
	struct ProtocolDef;
	struct ExtensionDef;
	struct BreakableBracedBlock;
}

namespace Parser
{
	struct Pin
	{
		Pin() { }
		Pin(std::string f, uint64_t l, uint64_t c, uint64_t ln) : file(f), line(l), col(c), len(ln) { }

		std::string file;
		uint64_t line = 1;
		uint64_t col = 1;
		uint64_t len = 1;
	};
}

namespace Codegen
{
	enum class TypeKind
	{
		Invalid,
		Struct,
		Class,
		Enum,
		TypeAlias,
		Extension,
		Func,
		BuiltinType,
		Tuple,
		Protocol,
		Parametric,
	};

	typedef std::pair<fir::Value*, Ast::VarDecl*> SymbolPair_t;
	typedef std::map<std::string, SymbolPair_t> SymTab_t;

	typedef std::pair<Ast::Expr*, TypeKind> TypedExpr_t;
	typedef std::pair<fir::Type*, TypedExpr_t> TypePair_t;
	typedef std::map<std::string, TypePair_t> TypeMap_t;

	typedef std::pair<Ast::BreakableBracedBlock*, std::pair<fir::IRBlock*, fir::IRBlock*>> BracedBlockScope;

	struct CodegenInstance;
	struct FunctionTree;


	struct FuncDefPair
	{
		explicit FuncDefPair(fir::Function* ffn, Ast::FuncDecl* fdecl, Ast::Func* afn) : firFunc(ffn), funcDecl(fdecl), funcDef(afn) { }

		static FuncDefPair empty() { return FuncDefPair(0, 0, 0); }

		bool isEmpty() { return this->firFunc == 0 && this->funcDef == 0; }
		bool operator == (const FuncDefPair& other) const { return this->firFunc == other.firFunc && this->funcDef == other.funcDef; }

		fir::Function* firFunc = 0;
		Ast::FuncDecl* funcDecl = 0;
		Ast::Func* funcDef = 0;
	};


	struct FunctionTree
	{
		FunctionTree() { this->id = __getnewid(); }
		explicit FunctionTree(std::string n) : nsName(n) { this->id = __getnewid(); }

		static id_t __getnewid()
		{
			static id_t curid = 0;
			return curid++;
		}

		id_t id;

		std::string nsName;
		std::deque<FunctionTree*> subs;

		// things within.
		std::deque<FuncDefPair> funcs;
		std::deque<Ast::OpOverload*> operators;
		std::deque<std::pair<Ast::FuncDecl*, Ast::Func*>> genericFunctions;

		std::map<std::string, TypePair_t> types;
		std::map<std::string, SymbolPair_t> vars;
		std::multimap<std::string, Ast::ExtensionDef*> extensions;
		std::map<std::string, Ast::ProtocolDef*> protocols;
	};

	struct Resolved_t
	{
		explicit Resolved_t(const FuncDefPair& fp) : t(fp), resolved(true) { }
		Resolved_t() : t(FuncDefPair::empty()), resolved(false) { }

		FuncDefPair t;
		bool resolved;
	};

	std::string unwrapPointerType(std::string type, int* indirections);
}


struct TypeConstraints_t
{
	std::deque<std::string> protocols;
	int pointerDegree = 0;

	bool operator == (const TypeConstraints_t& other) const
	{
		return this->protocols == other.protocols && this->pointerDegree == other.pointerDegree;
	}
};












