// repl.h
// Copyright (c) 2019, zhiayang
// Licensed under the Apache License Version 2.0.

#pragma once
#include "defs.h"

#include <optional>


namespace repl
{
	struct State;

	void start();
	void setupEnvironment();

	bool processLine(const std::string& line);
	std::optional<sst::Stmt*> parseAndTypecheck(const std::string& line, bool* needmore);

	void runCommand(const std::string& command);

	// used to save/restore if we wanna do weird things.
	void setEnvironment(State* st);
	State* getEnvironment();


	template <typename... Args>
	static void error(const std::string& fmt, Args&&... args)
	{
		fprintf(stderr, " %s*%s %s\n", COLOUR_RED_BOLD, COLOUR_RESET, zpr::sprint(fmt, args...).c_str());
	}

	template <typename... Args>
	static void log(const std::string& fmt, Args&&... args)
	{
		printf(" %s*%s %s\n", COLOUR_GREEN_BOLD, COLOUR_RESET, zpr::sprint(fmt, args...).c_str());
	}
}
