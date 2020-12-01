// frontend.h
// Copyright (c) 2014 - 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#pragma once

#include "precompile.h"

#include "lexer.h"
#include "parser.h"
#include "platform.h"

#include <stack>
#include <unordered_set>


namespace sst
{
	struct DefinitionTree;
}

namespace fir
{
	struct Module;
}

namespace backend
{
	enum class BackendOption;
	enum class ProgOutputMode;
	enum class OptimisationLevel;
}

namespace frontend
{
	std::string getParameter(const std::string& arg);
	std::string getVersion();

	backend::ProgOutputMode getOutputMode();
	backend::OptimisationLevel getOptLevel();
	backend::BackendOption getBackendOption();

	bool getPrintFIR();
	bool getPrintLLVMIR();

	bool getPrintProfileStats();
	bool getAbortOnError();

	bool getCanFFIEscape();
	bool getIsFreestanding();
	bool getIsNoStandardLibraries();
	bool getIsPositionIndependent();

	bool getIsNoRuntimeChecks();
	bool getIsNoRuntimeErrorStrings();

	bool getIsReplMode();

	std::vector<std::string> getFrameworksToLink();
	std::vector<std::string> getFrameworkSearchPaths();
	std::vector<std::string> getLibrariesToLink();
	std::vector<std::string> getLibrarySearchPaths();

	struct DependencyGraph;
	struct CollectorState
	{
		std::unordered_set<std::string> importedFiles;
		std::map<std::string, parser::ParsedFile> parsed;
		util::hash_map<std::string, sst::DefinitionTree*> dtrees;

		util::hash_map<std::string, parser::CustomOperatorDecl> binaryOps;
		util::hash_map<std::string, parser::CustomOperatorDecl> prefixOps;
		util::hash_map<std::string, parser::CustomOperatorDecl> postfixOps;

		DependencyGraph* graph = 0;
		std::string fullMainFile;
		std::vector<std::string> allFiles;

		size_t nativeWordSize = 0;

		// for statistics i guess
		size_t totalLinesOfCode = 0;
	};

	// fir::Module* collectFiles(std::string filename);
	void collectFiles(const std::string& mainfile, CollectorState* state);
	void parseFiles(CollectorState* state);
	sst::DefinitionTree* typecheckFiles(CollectorState* state);
	fir::Module* generateFIRModule(CollectorState* state, sst::DefinitionTree* maintree);


	std::pair<std::string, std::string> parseCmdLineOpts(int argc, char** argv);


	struct FileInnards
	{
		lexer::TokenList tokens;
		std::string_view fileContents;
		util::FastInsertVector<std::string_view> lines;
		std::vector<size_t> importIndices;

		bool didLex = false;
	};

	FileInnards& getFileState(const std::string& name);
	FileInnards lexTokensFromString(const std::string& fakename, const std::string_view& str);

	std::string getPathFromFile(const std::string& path);
	std::string getFilenameFromPath(const std::string& path);
	std::string getFullPathOfFile(const std::string& partial);
	std::string removeExtensionFromFilename(const std::string& name);

	void cachePreExistingFilename(const std::string& name);
	std::string getFileContents(const std::string& fullPath);
	const std::string& getFilenameFromID(size_t fileID);
	size_t getFileIDFromFilename(const std::string& name);
	lexer::TokenList& getFileTokens(const std::string& fullPath);
	const util::FastInsertVector<std::string_view>& getFileLines(size_t id);
	const std::vector<size_t>& getImportTokenLocationsForFile(const std::string& filename);

	std::string resolveImport(const std::string& imp, const Location& loc, const std::string& fullPath);

	struct ImportThing
	{
		std::string name;
		std::vector<std::string> importAs;
		bool pubImport = false;

		Location loc;
	};


	// dependency system
	struct DepNode
	{
		std::string name;

		// mainly to aid error reporting
		std::vector<std::pair<DepNode*, Location>> users;

		int index = -1;
		int lowlink = -1;
		bool onStack = false;
	};

	struct Dep
	{
		DepNode* from = 0;
		DepNode* to = 0;

		ImportThing ithing;
	};

	struct DependencyGraph
	{
		std::vector<DepNode*> nodes;
		std::map<DepNode*, std::vector<Dep*>> edgesFrom;

		std::stack<DepNode*> stack;

		std::vector<Dep*> getDependenciesOf(const std::string& name);
		void addModuleDependency(const std::string& from, const std::string& to, const ImportThing& ithing);
		std::vector<std::vector<DepNode*>> findCyclicDependencies();
	};

	std::vector<std::string> checkForCycles(const std::string& topmod, frontend::DependencyGraph* graph);
	frontend::DependencyGraph* buildDependencyGraph(frontend::DependencyGraph* graph, const std::string& full,
		util::hash_map<std::string, bool>& visited);
}






