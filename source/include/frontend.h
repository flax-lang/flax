// frontend.h
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once

#include "precompile.h"

#include "lexer.h"
#include "parser.h"

#include <unordered_set>

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

	struct CollectorState
	{
		std::unordered_set<std::string> importedFiles;
	};

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




// trash for windows
// todo: move this somewhere else maybe?
#ifdef _WIN32
#include <windows.h>
inline bool _fileExists(const TCHAR* szPath)
{
	DWORD dwAttrib = GetFileAttributes(szPath);
	return (dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

inline std::string _getFullPath(const char* pth)
{
	std::string path = pth;
	std::replace(path.begin(), path.end(), '/', '\\');

	char* out = new char[1024];
	GetFullPathName((TCHAR*) path.c_str(), 1024, (TCHAR*) out, 0);

	// windows is fucked up and GetFullPathName always returns non-zero, so we actually need to check if the
	// file exists by ourselves.

	bool exists = _fileExists((TCHAR*) out);
	if(!exists)
	{
		delete[] out;
		return "";
	}

	// ok
	std::string ret = out;
	delete[] out;

	return ret;
}

#else

inline std::string _getFullPath(const char* pth)
{
	auto ret = realpath(pth, 0);
	if(ret == 0) return "";
	return std::string(ret);
}

#endif




