// main.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include <iostream>
#include <fstream>
#include <cassert>
#include <sys/stat.h>
#include <sys/types.h>

#include "../include/ast.h"
#include "../include/parser.h"
#include "../include/codegen.h"
#include "../include/compiler.h"

#include "llvm/Support/TargetSelect.h"

#include "ir/type.h"
#include "ir/value.h"
#include "ir/module.h"
#include "ir/irbuilder.h"

using namespace Ast;

int main(int argc, char* argv[])
{
	// parse arguments
	auto names = Compiler::parseCmdLineArgs(argc, argv);

	std::string filename = names.first;
	std::string outname = names.second;

	// compile the file.
	// the file Compiler.cpp handles imports.

	iceAssert(llvm::InitializeNativeTarget() == 0);
	iceAssert(llvm::InitializeNativeTargetAsmParser() == 0);
	iceAssert(llvm::InitializeNativeTargetAsmPrinter() == 0);



	Codegen::CodegenInstance* __cgi = new Codegen::CodegenInstance();

	filename = Compiler::getFullPathOfFile(filename);
	std::string curpath = Compiler::getPathFromFile(filename);

	// parse and find all custom operators
	Parser::ParserState pstate(__cgi);

	Parser::parseAllCustomOperators(pstate, filename, curpath);

	// ret = std::tuple<Root*, std::vector<std::string>, std::hashmap<std::string, Root*>, std::hashmap<fir::Module*>>
	auto ret = Compiler::compileFile(filename, __cgi->customOperatorMap, __cgi->customOperatorMapRev);

	Compiler::compileToLlvm(filename, outname, ret);

	delete __cgi;
}


















