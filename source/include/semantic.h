// semantic.h
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once

#include <stdint.h>
#include <string>

namespace Codegen
{
	struct CodegenInstance;
}

namespace Ast
{
	struct Root;
	struct Expr;
}

enum class VarState
{
	Invalid = 0,

	NoValue,
	ValidAlloc,
	ValidStack,
	Deallocated,
	ModifiedAlloc,
};

struct VarDef
{
	std::string name;
	VarState state;

	Ast::Expr* expr;
};


namespace SemAnalysis
{
	void rewriteDotOperators(Codegen::CodegenInstance* cgi);
	void analyseVarUsage(Codegen::CodegenInstance* cgi);
}


























