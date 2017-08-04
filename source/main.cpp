// main.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "defs.h"
#include "errors.h"
#include "frontend.h"

int main(int argc, char** argv)
{
	auto [ input_file, output_file ] = frontend::parseCmdLineOpts(argc, argv);

	frontend::collectFiles(input_file);


	return 0;
}
