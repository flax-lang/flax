// x64AsmBackend.cpp
// Copyright (c) 2014 - 2016, zhiayang
// Licensed under the Apache License Version 2.0.

#include "backend.h"

namespace backend
{
	x64Backend::x64Backend(CompiledData& dat, const std::vector<std::string>& inputs, const std::string& output)
		: Backend(0, dat, inputs, output)
	{
	}

	void x64Backend::performCompilation()
	{
		_error_and_exit("enotsup\n");
	}

	void x64Backend::optimiseProgram()
	{
		_error_and_exit("enotsup\n");
	}

	void x64Backend::writeOutput()
	{
		_error_and_exit("enotsup\n");
	}

	std::string x64Backend::str()
	{
		return "x64 Assembly";
	}
}
