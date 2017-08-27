// frontend.h
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once

#include "precompile.h"

#include "lexer.h"
#include "parser.h"

namespace ast
{
	struct Expr;
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
	std::string getParameter(std::string arg);

	backend::ProgOutputMode getOutputMode();
	backend::OptimisationLevel getOptLevel();
	backend::BackendOption getBackendOption();

	bool getIsFreestanding();
	bool getIsPositionIndependent();
	std::vector<std::string> getFrameworksToLink();
	std::vector<std::string> getFrameworkSearchPaths();
	std::vector<std::string> getLibrariesToLink();
	std::vector<std::string> getLibrarySearchPaths();


	fir::Module* collectFiles(std::string filename);

	std::pair<std::string, std::string> parseCmdLineOpts(int argc, char** argv);



	std::string getPathFromFile(std::string path);
	std::string getFilenameFromPath(std::string path);
	std::string getFullPathOfFile(std::string partial);
	std::string removeExtensionFromFilename(std::string name);

	std::string getFileContents(std::string fullPath);
	const std::string& getFilenameFromID(size_t fileID);
	size_t getFileIDFromFilename(const std::string& name);
	lexer::TokenList& getFileTokens(std::string fullPath);
	const util::FastVector<stx::string_view>& getFileLines(size_t id);
	const std::vector<size_t>& getImportTokenLocationsForFile(const std::string& filename);

	std::string resolveImport(std::string imp, const Location& loc, std::string fullPath);

	// dependency system
	struct DepNode
	{
		std::string name;
		Location loc;

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
	};

	struct DependencyGraph
	{
		std::vector<DepNode*> nodes;
		std::map<DepNode*, std::vector<Dep*>> edgesFrom;

		std::stack<DepNode*> stack;

		std::vector<DepNode*> getDependenciesOf(std::string name);
		void addModuleDependency(std::string from, std::string to, const Location& loc);
		std::vector<std::vector<DepNode*>> findCyclicDependencies();
	};

	std::vector<std::string> checkForCycles(std::string topmod, frontend::DependencyGraph* graph);
	frontend::DependencyGraph* buildDependencyGraph(frontend::DependencyGraph* graph, std::string full,
		std::unordered_map<std::string, bool>& visited);
}






