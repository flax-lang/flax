// main.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include <iostream>
#include <fstream>
#include <cassert>

#include "include/ast.h"
using namespace Ast;


int main(int argc, char* argv[])
{
	if(argc > 1)
	{
		printf("Parsing file %s\n", argv[1]);

		// open the file.
		std::ifstream file = std::ifstream(argv[1]);
		std::stringstream stream;

		stream << file.rdbuf();
		std::string str = stream.str();

		// parse
		Root* root = Parser::Parse(argv[1], str);

		printf("llvm ir:\n\n");
		Codegen::doCodegen(root);
	}
	else
	{
		fprintf(stderr, "Expected at least one argument\n");
	}
}





