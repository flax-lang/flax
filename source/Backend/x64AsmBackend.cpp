// x64AsmBackend.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "backend.h"

namespace Compiler
{
	x64Backend::x64Backend(CompiledData& dat, std::deque<std::string> inputs, std::string output)
		: Backend(0, dat, inputs, output)
	{
	}

	void x64Backend::performCompilation()
	{
		error_and_exit("enotsup");
	}

	void x64Backend::optimiseProgram()
	{
		error_and_exit("enotsup");
	}

	void x64Backend::writeOutput()
	{
		error_and_exit("enotsup");
	}

	std::string x64Backend::str()
	{
		return "x64 Assembly";
	}
}
