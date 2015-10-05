// defs.h
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string>
#include <map>
#include <unordered_map>
#include <deque>

#include <sys/types.h>

// forward declarations.
namespace llvm
{
	class Value;
	class Type;
	class Function;
	class BasicBlock;
}

namespace Ast
{
	struct Expr;
	struct VarDecl;
	struct FuncDecl;
	struct BreakableBracedBlock;
}

namespace Parser
{
	struct PosInfo
	{
		PosInfo() { }

		uint64_t line = 1;
		uint64_t col = 1;
		std::string file;
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

	typedef std::pair<llvm::Value*, Ast::VarDecl*> SymbolPair_t;
	typedef std::map<std::string, SymbolPair_t> SymTab_t;

	typedef std::pair<Ast::Expr*, TypeKind> TypedExpr_t;
	typedef std::pair<llvm::Type*, TypedExpr_t> TypePair_t;
	typedef std::map<std::string, TypePair_t> TypeMap_t;

	typedef std::pair<llvm::Function*, Ast::FuncDecl*> FuncPair_t;
	// typedef std::map<std::string, FuncPair_t> FuncMap_t;

	typedef std::pair<Ast::BreakableBracedBlock*, std::pair<llvm::BasicBlock*, llvm::BasicBlock*>> BracedBlockScope;

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












