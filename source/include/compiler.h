// compiler.h
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include <string>
#include <vector>
#include <map>

#pragma once

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
		std::vector<std::string> fileList;

		std::unordered_map<std::string, Ast::Root*> rootMap;
		std::deque<std::pair<std::string, fir::Module*>> moduleList;


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

	std::deque<std::deque<Codegen::DepNode*>> checkCyclicDependencies(std::string filename);

	CompiledData compileFile(std::string filename,std::deque<std::deque<Codegen::DepNode*>> groups,
		std::map<Ast::ArithmeticOp, std::pair<std::string, int>> foundOps, std::map<std::string, Ast::ArithmeticOp> foundOpsRev);

	void compileToLlvm(std::string filename, std::string outname, CompiledData data);









	void writeBitcode(std::string filename, llvm::Module* cgi);

	// final stages
	void compileProgram(llvm::Module* module, std::vector<std::string> filelist, std::string foldername, std::string outname);

	std::string resolveImport(Ast::Import* imp, std::string fullPath);


	std::deque<Parser::Token> getFileTokens(std::string fullPath);
	std::vector<std::string> getFileLines(std::string fullPath);
	std::string getFileContents(std::string fullPath);

	std::string getPathFromFile(std::string path);
	std::string getFilenameFromPath(std::string path);
	std::string getFullPathOfFile(std::string partial);



	std::pair<std::string, std::string> parseCmdLineArgs(int argc, char** argv);


	bool getDumpFir();
	bool getDumpLlvm();
	std::string getTarget();
	std::string getPrefix();
	std::string getMcModel();
	std::string getSysroot();

	bool getIsCompileOnly();
	int getOptimisationLevel();
	bool getPrintClangOutput();
	bool getRunProgramWithJit();
	bool getIsPositionIndependent();
	bool getNoAutoGlobalConstructor();


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
}







