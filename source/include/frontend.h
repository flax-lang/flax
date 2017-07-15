// frontend.h
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include <stdint.h>

#include <algorithm>
#include <string>

namespace frontend
{
	enum class OptimisationLevel
	{
		Invalid,

		Debug,		// -Ox
		None,		// -O0

		Minimal,	// -O1
		Normal,		// -O2
		Aggressive	// -O3
	};

	enum class ProgOutputMode
	{
		Invalid,

		RunJit,			// -run or -jit
		ObjectFile,		// -c
		LLVMBitcode,	// -emit-llvm
		Program			// (default)
	};

	enum class BackendOption
	{
		Invalid,

		LLVM,
		Assembly_x64,
	};

	std::pair<std::string, std::string> parseCmdLineOpts(int argc, char** argv);
}
