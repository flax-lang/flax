// main.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include <iostream>
#include <fstream>
#include <cassert>

#include "include/parser.h"

int main(int argc, char* argv[])
{
	assert(argc > 1);
	printf("Parsing file %s\n\n", argv[1]);

	Parser::Parse(std::string(argv[1]));
}
