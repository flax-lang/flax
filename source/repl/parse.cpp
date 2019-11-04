// parse.cpp
// Copyright (c) 2019, zhiayang
// Licensed under the Apache License Version 2.0.

#include "repl.h"
#include "parser.h"
#include "frontend.h"

namespace repl
{
	void processLine(const std::string& line)
	{
		frontend::CollectorState collector;

		// lex.
		platform::cachePreExistingFile("<repl>", line);
		auto lexResult = frontend::lexTokensFromString("<repl>", line);

		// parse
		auto parseResult = parser::parseFile("<repl>", lexResult, collector);
	}
}
