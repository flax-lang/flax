// repl.h
// Copyright (c) 2019, zhiayang
// Licensed under the Apache License Version 2.0.

#pragma once
#include "defs.h"

namespace repl
{
	void start();
	void setupEnvironment();
	bool processLine(const std::string& line);


	template <typename... Args>
	static void error(const std::string& fmt, Args&&... args)
	{
		fprintf(stderr, "%serror:%s %s\n", COLOUR_RED_BOLD, COLOUR_RESET, zpr::sprint(fmt, args...).c_str());
	}
}
