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
	struct TypeDefn;
	struct FuncDefn;
	struct FunctionCall;
	struct Parameterisable;
}

namespace fir
{
	struct ConstantNumberType;
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
		std::unordered_map<std::string, std::vector<ast::Parameterisable*>> unresolvedGenericDefs;
		std::unordered_map<std::pair<ast::Parameterisable*, std::unordered_map<std::string, TypeConstraints_t>>, sst::Defn*> resolvedGenericDefs;

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
		std::vector<ast::Parameterisable*> getUnresolvedGenericDefnsWithName(const std::string& name);

		void addDefinition(const std::string& name, Defn* def);
		void addDefinition(const std::string& sourceFile, const std::string& name, Defn* def);
	};

	struct DefinitionTree
	{
		DefinitionTree(StateTree* st) : base(st) { }

		StateTree* base = 0;
		NamespaceDefn* topLevel = 0;
		std::unordered_set<std::string> thingsImported;

		std::unordered_map<fir::Type*, TypeDefn*> typeDefnMap;
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

		std::vector<int> bodyStack;
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


		// std::vector<TypeParamMap_t> genericTypeStack;
		// std::vector<TypeParamMap_t> getGenericTypeStack();

		std::vector<TypeParamMap_t> genericContextStack;
		std::vector<TypeParamMap_t> getGenericContextStack();

		// void pushGenericType();
		// void popGenericType();

		void pushGenericContext();
		fir::Type* findGenericMapping(const std::string& name, bool allowFail);
		void addGenericMapping(const std::string& name, fir::Type* ty);
		void removeGenericMapping(const std::string& name);
		void popGenericContext();


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
		StateTree* getTreeOfScope(const std::vector<std::string>& scope);

		std::vector<Defn*> getDefinitionsWithName(const std::string& name, StateTree* tree = 0);
		bool checkForShadowingOrConflictingDefinition(Defn* def, std::function<bool (TypecheckState* fs, Defn* other)> checkConflicting, StateTree* tree = 0);

		fir::Type* getBinaryOpResultType(fir::Type* a, fir::Type* b, const std::string& op, sst::FunctionDefn** overloadFn = 0);

		// things that i might want to make non-methods someday
		fir::Type* convertParserTypeToFIR(pts::Type* pt, bool allowFailure = false);
		fir::Type* inferCorrectTypeForLiteral(fir::ConstantNumberType* lit);
		TypeParamMap_t convertParserTypeArgsToFIR(const std::unordered_map<std::string, pts::Type*>& gmaps, bool allowFailure = false);


		fir::Type* checkIsBuiltinConstructorCall(const std::string& name, const std::vector<FnCallArgument>& arguments);


		bool checkAllPathsReturn(FunctionDefn* fn);


		std::tuple<TypeParamMap_t, std::unordered_map<fir::Type*, fir::Type*>, SimpleError> inferTypesForGenericEntity(ast::Parameterisable* target,
			std::vector<FnCallArgument>& input, const TypeParamMap_t& partial, fir::Type* infer, bool isFnCall);


		//* gets an generic thing in the AST form and returns a concrete SST node from it, given the mappings.
		TCResult instantiateGenericEntity(ast::Parameterisable* type, const TypeParamMap_t& mappings);

		TCResult attemptToDisambiguateGenericReference(const std::string& name, const std::vector<ast::Parameterisable*>& gdefs,
			const TypeParamMap_t& mappings, fir::Type* infer, bool isFnCall, std::vector<FnCallArgument>& args);

		//* basically does the work that makes 'using' actually 'use' stuff. Imports everything in _from_ to _to_.
		void importScopeContentsIntoExistingScope(const std::vector<std::string>& from, const std::vector<std::string>& to);

		//* kind of like the above, but subtly different in that we create a *new scope* named _name_ in the scope _toParent_
		void importScopeContentsIntoNewScope(const std::vector<std::string>& from, const std::vector<std::string>& toParent, const std::string& name);

		std::vector<FnCallArgument> typecheckCallArguments(const std::vector<std::pair<std::string, ast::Expr*>>& args);

		DecompMapping typecheckDecompositions(const DecompMapping& bind, fir::Type* rhs, bool immut, bool allowref);

		int getCastDistance(fir::Type* from, fir::Type* to);

		int getOverloadDistance(const std::vector<fir::Type*>& a, const std::vector<fir::Type*>& b);
		int getOverloadDistance(const std::vector<FunctionDecl::Param>& a, const std::vector<FunctionDecl::Param>& b);

		bool isDuplicateOverload(const std::vector<fir::Type*>& a, const std::vector<fir::Type*>& b);
		bool isDuplicateOverload(const std::vector<FunctionDecl::Param>& a, const std::vector<FunctionDecl::Param>& b);

		TCResult resolveFunctionCall(const std::string& name, std::vector<FnCallArgument>& arguments, const TypeParamMap_t& gmaps,
			bool traverseUp, fir::Type* inferredRetType);
		TCResult resolveFunctionCallFromCandidates(const std::vector<Defn*>& fs, const std::vector<FunctionDecl::Param>& arguments, const TypeParamMap_t& gmaps,
			bool allowImplicitSelf);

		TCResult resolveConstructorCall(TypeDefn* defn, const std::vector<FunctionDecl::Param>& arguments, const TypeParamMap_t& gmaps);
	};

	DefinitionTree* typecheck(frontend::CollectorState* cs, const parser::ParsedFile& file,
		const std::vector<std::pair<frontend::ImportThing, StateTree*>>& imports);
}















