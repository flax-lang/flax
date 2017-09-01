// typecheck.h
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once
#include "defs.h"
#include "sst.h"

#include "precompile.h"

namespace parser
{
	struct ParsedFile;
}

namespace pts
{
	struct Type;
}

namespace ast
{
	struct Stmt;
	struct FuncDefn;
}

namespace sst
{
	struct StateTree
	{
		StateTree(std::string nm, StateTree* p) : name(nm), parent(p) { }

		std::string name;
		StateTree* parent = 0;

		std::unordered_map<std::string, std::vector<Defn*>> definitions;

		// std::unordered_map<std::string, VarDefn*> variables;
		// std::unordered_map<std::string, std::vector<FunctionDefn*>> functions;
		// std::unordered_map<std::string, ForeignFuncDefn*> foreignFunctions;

		std::unordered_map<std::string, std::vector<ast::FuncDefn*>> unresolvedGenericFunctions;

		std::unordered_map<std::string, StateTree*> subtrees;
	};

	struct DefinitionTree
	{
		DefinitionTree(StateTree* st) : base(st) { }

		StateTree* base = 0;
		NamespaceDefn* topLevel = 0;
		std::vector<std::string> thingsImported;
	};

	struct TypecheckState
	{
		TypecheckState(StateTree* st) : dtree(new DefinitionTree(st)), stree(dtree->base) { }

		std::string moduleName;
		sst::NamespaceDefn* topLevelNamespace = 0;

		DefinitionTree* dtree = 0;
		StateTree*& stree;

		std::vector<std::unordered_map<std::string, VarDefn*>> symbolTableStack;

		std::vector<Location> locationStack;

		void pushLoc(const Location& l);
		void pushLoc(ast::Stmt* stmt);

		int functionNestLevel = 0;
		bool isInFunctionBody();
		void enterFunctionBody();
		void exitFunctionBody();

		Location loc();
		Location popLoc();

		void pushTree(std::string name);
		StateTree* popTree();

		std::string serialiseCurrentScope();
		std::vector<std::string> getCurrentScope();

		// std::vector<FunctionDefn*> getFunctionsWithName(std::string name, StateTree* scope = 0);
		// std::vector<FunctionDecl*> getFunctionDeclsWithName(std::string name, StateTree* scope = 0);

		std::vector<Stmt*> getDefinitionsWithName(std::string name, StateTree* tree = 0);
		bool checkForShadowingOrConflictingDefinition(Defn* def, std::string kind,
			std::function<bool (TypecheckState* fs, Defn* other)> checkConflicting, StateTree* tree = 0);

		fir::Type* getBinaryOpResultType(fir::Type* a, fir::Type* b, Operator op);

		// things that i might want to make non-methods someday
		fir::Type* convertParserTypeToFIR(pts::Type* pt);
		fir::Type* inferCorrectTypeForLiteral(Expr* lit);

		struct PrettyError
		{
			std::string errorStr;
			std::vector<std::pair<Location, std::string>> infoStrs;
		};

		FunctionDecl* resolveFunction(std::string name, std::vector<FunctionDecl::Param> arguments, PrettyError* errs);
		FunctionDecl* resolveFunctionFromCandidates(std::vector<FunctionDecl*> fs, std::vector<FunctionDecl::Param> arguments,
			PrettyError* errs);
	};

	DefinitionTree* typecheck(const parser::ParsedFile& file, std::vector<std::pair<std::string, StateTree*>> imports);
}















