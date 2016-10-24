// main.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include <iostream>
#include <fstream>
#include <cassert>
#include <sys/stat.h>
#include <sys/types.h>

#include "ast.h"
#include "parser.h"
#include "backend.h"
#include "codegen.h"
#include "compiler.h"
#include "dependency.h"

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


	auto groups = Compiler::checkCyclicDependencies(filename);

	Parser::parseAllCustomOperators(pstate, filename, curpath);

	// ret = std::tuple<Root*, std::vector<std::string>, std::hashmap<std::string, Root*>, std::hashmap<fir::Module*>>
	auto cd = Compiler::compileFile(filename, groups, __cgi->customOperatorMap, __cgi->customOperatorMapRev);

	// do FIR optimisations
	for(auto mod : cd.moduleList)
	{
		for(auto f : mod.second->getAllFunctions())
			f->cullUnusedValues();

		if(Compiler::getDumpFir())
			printf("%s\n\n", mod.second->print().c_str());
	}



	// check caps that we need
	using namespace Compiler;
	int capsneeded = 0;
	{
		if(getOutputMode() == ProgOutputMode::RunJit)
			capsneeded |= BackendCaps::JIT;

		if(getOutputMode() == ProgOutputMode::ObjectFile)
			capsneeded |= BackendCaps::EmitObject;

		if(getOutputMode() == ProgOutputMode::Program)
			capsneeded |= BackendCaps::EmitProgram;
	}

	Backend* backend = Backend::getBackendFromOption(getSelectedBackend(), cd, { filename }, outname);

	if(backend->hasCapability((BackendCaps::Capabilities) capsneeded))
	{
		backend->performCompilation();
		backend->optimiseProgram();
		backend->writeOutput();
	}
	else
	{
		fprintf(stderr, "Selected backend '%s' does not have some required capabilities (missing '%s')\n", backend->str().c_str(),
			capabilitiesToString((BackendCaps::Capabilities) capsneeded).c_str());

		exit(-1);
	}

	delete __cgi;
}



















