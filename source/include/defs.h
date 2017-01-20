// defs.h
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>

#include <map>
#include <deque>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

#include <sys/types.h>

#define TAB_WIDTH	4

#include "iceassert.h"
#include "ir/identifier.h"

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
		Pin() : fileID(0), line(1), col(1), len(0) { }
		Pin(size_t f, size_t l, size_t c, size_t ln) : fileID(f), line(l), col(c), len(ln) { }

		size_t fileID;
		size_t line;
		size_t col;
		size_t len;
	};

	struct Token;

	using TokenList = std::vector<Token>;
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
		Array
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
		FunctionTree(FunctionTree* p) : parent(p) { this->id = __getnewid(); }
		explicit FunctionTree(std::string n, FunctionTree* p) : nsName(n), parent(p) { this->id = __getnewid(); }

		static size_t __getnewid()
		{
			static size_t curid = 0;
			return curid++;
		}

		size_t id;

		std::string nsName;
		FunctionTree* parent;

		std::vector<FunctionTree*> subs;
		std::unordered_map<std::string, FunctionTree*> subMap;	// purely for fast duplicate checking

		// things within.
		std::vector<FuncDefPair> funcs;
		std::unordered_set<Identifier> funcSet;		// purely for fast duplicate checking during import


		std::vector<Ast::OpOverload*> operators;
		std::vector<std::pair<Ast::FuncDecl*, Ast::Func*>> genericFunctions;

		std::unordered_map<std::string, TypePair_t> types;
		std::unordered_map<std::string, SymbolPair_t> vars;
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
	std::vector<std::string> protocols;
	int pointerDegree = 0;

	bool operator == (const TypeConstraints_t& other) const
	{
		return this->protocols == other.protocols && this->pointerDegree == other.pointerDegree;
	}
};




#define COLOUR_RESET			"\033[0m"
#define COLOUR_BLACK			"\033[30m"			// Black
#define COLOUR_RED				"\033[31m"			// Red
#define COLOUR_GREEN			"\033[32m"			// Green
#define COLOUR_YELLOW			"\033[33m"			// Yellow
#define COLOUR_BLUE				"\033[34m"			// Blue
#define COLOUR_MAGENTA			"\033[35m"			// Magenta
#define COLOUR_CYAN				"\033[36m"			// Cyan
#define COLOUR_WHITE			"\033[37m"			// White
#define COLOUR_BLACK_BOLD		"\033[1m"			// Bold Black
#define COLOUR_RED_BOLD			"\033[1m\033[31m"	// Bold Red
#define COLOUR_GREEN_BOLD		"\033[1m\033[32m"	// Bold Green
#define COLOUR_YELLOW_BOLD		"\033[1m\033[33m"	// Bold Yellow
#define COLOUR_BLUE_BOLD		"\033[1m\033[34m"	// Bold Blue
#define COLOUR_MAGENTA_BOLD		"\033[1m\033[35m"	// Bold Magenta
#define COLOUR_CYAN_BOLD		"\033[1m\033[36m"	// Bold Cyan
#define COLOUR_WHITE_BOLD		"\033[1m\033[37m"	// Bold White
#define COLOUR_GREY_BOLD		"\033[30;1m"		// Bold Grey











