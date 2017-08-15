// typecheck.h
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once
#include "defs.h"
#include "sst.h"

#include <unordered_map>

namespace parser
{
	struct ParsedFile;
}

namespace pts
{
	struct Type;
}

namespace sst
{
	struct StateTree
	{
		std::string name;

		std::unordered_map<std::string, VarDefn*> variables;
		std::unordered_map<std::string, FunctionDefn*> functions;
		std::unordered_map<std::string, ForeignFuncDefn*> foreignFunctions;

		std::unordered_map<std::string, StateTree*> subtrees;
	};

	struct TypecheckState
	{
		sst::NamespaceDefn* topLevelNamespace = 0;

		StateTree* stree = 0;
		std::vector<std::unordered_map<std::string, VarDefn*>> symbolTableStack;

		std::vector<Location> locationStack;

		void pushLoc(const Location& l);
		Location loc();
		Location popLoc();

		fir::Type* convertParserTypeToFIR(pts::Type* pt);
	};

	struct WholeSemanticState
	{

	};












	TypecheckState* typecheck(const parser::ParsedFile& file);
	void addFileToWhole(WholeSemanticState* whole, TypecheckState* file);
}
