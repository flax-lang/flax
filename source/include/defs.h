// defs.h
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string>
#include <map>
#include <unordered_map>
#include <deque>

#include <sys/types.h>

#define TAB_WIDTH	4

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
	};

	typedef std::pair<fir::Value*, Ast::VarDecl*> SymbolPair_t;
	typedef std::map<std::string, SymbolPair_t> SymTab_t;

	typedef std::pair<Ast::Expr*, TypeKind> TypedExpr_t;
	typedef std::pair<fir::Type*, TypedExpr_t> TypePair_t;
	typedef std::map<std::string, TypePair_t> TypeMap_t;

	typedef std::pair<fir::Function*, Ast::FuncDecl*> FuncPair_t;
	// typedef std::map<std::string, FuncPair_t> FuncMap_t;

	typedef std::pair<Ast::BreakableBracedBlock*, std::pair<fir::IRBlock*, fir::IRBlock*>> BracedBlockScope;

	struct CodegenInstance;
	struct FunctionTree;


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
		std::deque<FuncPair_t> funcs;
		std::deque<Ast::OpOverload*> operators;
		std::deque<std::pair<Ast::FuncDecl*, Ast::Func*>> genericFunctions;

		std::map<std::string, SymbolPair_t> vars;
		std::map<std::string, TypePair_t> types;
	};

	struct Resolved_t
	{
		explicit Resolved_t(const FuncPair_t& fp) : t(fp), resolved(true) { }
		Resolved_t() : resolved(false) { }

		FuncPair_t t;
		bool resolved;
	};
}












