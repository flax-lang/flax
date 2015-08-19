// defs.h
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include <stdint.h>
#include <stddef.h>
#include <string>
#include <map>

#include "llvm_all.h"

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

		uint64_t line = 0;
		uint64_t col = 0;
		std::string file;
	};
}

namespace Codegen
{
	enum class TypeKind
	{
		Struct,
		Enum,
		TypeAlias,
		Func,
		BuiltinType,
		Tuple,
		Protocol,
	};

	enum class SymbolValidity
	{
		Valid,
		UseAfterDealloc
	};


	typedef std::pair<llvm::Value*, SymbolValidity> SymbolValidity_t;
	typedef std::pair<SymbolValidity_t, Ast::VarDecl*> SymbolPair_t;
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
		FunctionTree() { }
		FunctionTree(std::string n) : nsName(n) { }

		std::string nsName;
		std::deque<FunctionTree*> subs;

		// things within.
		std::deque<FuncPair_t> funcs;
		std::deque<SymbolPair_t> vars;
	};

	struct Resolved_t
	{
		Resolved_t(const FuncPair_t& fp) : t(fp), resolved(true) { }
		Resolved_t() : resolved(false) { }

		FuncPair_t t;
		bool resolved;
	};
}












