// compiler.h
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
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

namespace Compiler
{
	Ast::Root* compileFile(std::string filename, std::vector<std::string>& filenames, std::map<std::string, Ast::Root*>& rootmap, std::vector<llvm::Module*>& modules, Codegen::CodegenInstance* cgi);

	// final stages
	void compileProgram(Codegen::CodegenInstance* cgi, std::vector<std::string> filelist, std::string foldername, std::string outname);

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
	bool getDisableLowercaseBuiltinTypes();

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
	#define COLOUR_BLACK_BOLD		"\033[1m"	// Bold Black
	#define COLOUR_RED_BOLD			"\033[1m\033[31m"	// Bold Red
	#define COLOUR_GREEN_BOLD		"\033[1m\033[32m"	// Bold Green
	#define COLOUR_YELLOW_BOLD		"\033[1m\033[33m"	// Bold Yellow
	#define COLOUR_BLUE_BOLD		"\033[1m\033[34m"	// Bold Blue
	#define COLOUR_MAGENTA_BOLD		"\033[1m\033[35m"	// Bold Magenta
	#define COLOUR_CYAN_BOLD		"\033[1m\033[36m"	// Bold Cyan
	#define COLOUR_WHITE_BOLD		"\033[1m\033[37m"	// Bold White
}







