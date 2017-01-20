// compiler.h
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once

#include "ast.h"
#include <deque>
#include <experimental/string_view>

namespace llvm
{
	class Module;
}

namespace fir
{
	struct Module;
}

namespace Parser
{
	struct ParserState;
	struct Token;
}

namespace Codegen
{
	struct DepNode;
	struct DependencyGraph;
}

namespace Compiler
{
	struct CompiledData
	{
		Ast::Root* rootNode = 0;

		std::unordered_map<std::string, Ast::Root*> rootMap;
		std::vector<std::pair<std::string, fir::Module*>> moduleList;


		fir::Module* getModule(std::string name)
		{
			for(auto pair : this->moduleList)
			{
				if(pair.first == name)
					return pair.second;
			}

			return 0;
		}
	};

	std::vector<std::vector<Codegen::DepNode*>> checkCyclicDependencies(std::string filename);

	CompiledData compileFile(std::string filename,std::vector<std::vector<Codegen::DepNode*>> groups,
		std::map<Ast::ArithmeticOp, std::pair<std::string, int>> foundOps, std::map<std::string, Ast::ArithmeticOp> foundOpsRev);

	std::string resolveImport(Ast::Import* imp, std::string fullPath);


	std::string getFileContents(std::string fullPath);
	Parser::TokenList& getFileTokens(std::string fullPath);
	const std::vector<std::experimental::string_view>& getFileLines(size_t id);

	std::string getPathFromFile(std::string path);
	std::string getFilenameFromPath(std::string path);
	std::string getFullPathOfFile(std::string partial);

	const std::string& getFilenameFromID(size_t fileID);
	size_t getFileIDFromFilename(const std::string& name);

	std::pair<std::string, std::string> parseCmdLineArgs(int argc, char** argv);


	bool getDumpFir();
	bool getDumpLlvm();
	bool getEmitLLVMOutput();
	bool showProfilerOutput();
	std::string getTarget();
	std::string getPrefix();
	std::string getCodeModel();
	std::string getSysroot();

	std::vector<std::string> getLibrarySearchPaths();
	std::vector<std::string> getLibrariesToLink();
	std::vector<std::string> getFrameworksToLink();
	std::vector<std::string> getFrameworkSearchPaths();

	enum class BackendOption;
	enum class OptimisationLevel;
	enum class ProgOutputMode;


	ProgOutputMode getOutputMode();
	BackendOption getSelectedBackend();
	OptimisationLevel getOptimisationLevel();

	bool getPrintClangOutput();
	bool getIsPositionIndependent();
	bool getNoAutoGlobalConstructor();


	bool getIsFreestandingMode();


	enum class Flag
	{
		WarningsAsErrors		= 0x1,
		NoWarnings				= 0x2,
	};

	enum class Warning
	{
		UnusedVariable,
		UseBeforeAssign,
		UseAfterFree,
	};

	bool getFlag(Flag f);
	bool getWarningEnabled(Warning warning);
}







