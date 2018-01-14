// typecheck.h
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once
#include "sst.h"
#include "frontend.h"

#include "precompile.h"

#include <unordered_set>

namespace parser
{
	struct ParsedFile;
}

namespace frontend
{
	struct CollectorState;
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
		StateTree(const std::string& nm, const std::string& filename, StateTree* p) : name(nm), topLevelFilename(filename), parent(p) { }

		std::string name;
		std::string topLevelFilename;

		StateTree* parent = 0;

		std::unordered_map<std::string, StateTree*> subtrees;
		std::unordered_map<std::string, std::vector<ast::Stmt*>> unresolvedGenericDefs;

		using DefnMap = std::unordered_map<std::string, std::vector<Defn*>>;

		// maps from filename to defnmap -- allows tracking definitions by where they came from
		// so we can resolve the import duplication bullshit
		std::unordered_map<std::string, DefnMap> definitions;

		// what's there to explain? a simple map of operators to their functions. we use
		// function overload resolution to determine which one to call, and ambiguities are
		// handled the usual way.
		std::unordered_map<std::string, std::vector<sst::FunctionDefn*>> infixOperatorOverloads;
		std::unordered_map<std::string, std::vector<sst::FunctionDefn*>> prefixOperatorOverloads;
		std::unordered_map<std::string, std::vector<sst::FunctionDefn*>> postfixOperatorOverloads;

		std::vector<std::string> getScope();
		StateTree* searchForName(const std::string& name);

		DefnMap getAllDefinitions();

		std::vector<Defn*> getDefinitionsWithName(const std::string& name);
		void addDefinition(const std::string& name, Defn* def);
		void addDefinition(const std::string& sourceFile, const std::string& name, Defn* def);
	};

	struct DefinitionTree
	{
		DefinitionTree(StateTree* st) : base(st) { }

		StateTree* base = 0;
		NamespaceDefn* topLevel = 0;
		std::unordered_set<std::string> thingsImported;
	};

	struct TypecheckState
	{
		TypecheckState(StateTree* st) : dtree(new DefinitionTree(st)), stree(dtree->base) { }

		std::string moduleName;

		DefinitionTree* dtree = 0;
		StateTree*& stree;

		std::unordered_map<fir::Type*, TypeDefn*> typeDefnMap;

		std::vector<std::unordered_map<std::string, VarDefn*>> symbolTableStack;

		std::vector<Location> locationStack;

		// void pushLoc(const Location& l);
		void pushLoc(ast::Stmt* stmt);

		std::vector<FunctionDefn*> currentFunctionStack;
		bool isInFunctionBody();

		FunctionDefn* getCurrentFunction();
		void enterFunctionBody(FunctionDefn* fn);
		void leaveFunctionBody();


		std::vector<TypeDefn*> structBodyStack;
		TypeDefn* getCurrentStructBody();
		bool isInStructBody();
		void enterStructBody(TypeDefn* str);
		void leaveStructBody();


		int breakableBodyNest = 0;
		void enterBreakableBody();
		void leaveBreakableBody();
		bool isInBreakableBody();

		int deferBlockNest = 0;
		void enterDeferBlock();
		void leaveDeferBlock();
		bool isInDeferBlock();

		std::string getAnonymousScopeName();

		Location loc();
		Location popLoc();

		void pushTree(const std::string& name);
		StateTree* popTree();

		StateTree* recursivelyFindTreeUpwards(const std::string& name);

		std::string serialiseCurrentScope();
		std::vector<std::string> getCurrentScope();
		void teleportToScope(const std::vector<std::string>& scope);

		std::vector<Defn*> getDefinitionsWithName(const std::string& name, StateTree* tree = 0);
		bool checkForShadowingOrConflictingDefinition(Defn* def, const std::string& kind,
			std::function<bool (TypecheckState* fs, Defn* other)> checkConflicting, StateTree* tree = 0);

		fir::Type* getBinaryOpResultType(fir::Type* a, fir::Type* b, const std::string& op, sst::FunctionDefn** overloadFn = 0);

		// things that i might want to make non-methods someday
		fir::Type* convertParserTypeToFIR(pts::Type* pt);
		fir::Type* inferCorrectTypeForLiteral(Expr* lit);

		bool checkAllPathsReturn(FunctionDefn* fn);

		struct PrettyError
		{
			std::string errorStr;
			std::vector<std::pair<Location, std::string>> infoStrs;
		};

		int getOverloadDistance(const std::vector<fir::Type*>& a, const std::vector<fir::Type*>& b);
		int getOverloadDistance(const std::vector<FunctionDecl::Param>& a, const std::vector<FunctionDecl::Param>& b);

		bool isDuplicateOverload(const std::vector<fir::Type*>& a, const std::vector<fir::Type*>& b);
		bool isDuplicateOverload(const std::vector<FunctionDecl::Param>& a, const std::vector<FunctionDecl::Param>& b);

		Defn* resolveFunction(const std::string& name, const std::vector<FunctionDecl::Param>& arguments, PrettyError* errs, bool traverseUp);
		Defn* resolveFunctionFromCandidates(const std::vector<Defn*>& fs, const std::vector<FunctionDecl::Param>& arguments,
			PrettyError* errs, bool allowImplicitSelf);

		TypeDefn* resolveConstructorCall(TypeDefn* defn, const std::vector<FunctionDecl::Param>& arguments, PrettyError* errs);
	};

	DefinitionTree* typecheck(frontend::CollectorState* cs, const parser::ParsedFile& file,
		const std::vector<std::pair<frontend::ImportThing, StateTree*>>& imports);
}















