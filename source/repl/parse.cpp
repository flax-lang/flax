// parse.cpp
// Copyright (c) 2019, zhiayang
// Licensed under the Apache License Version 2.0.

#include "repl.h"
#include "parser.h"
#include "frontend.h"
#include "parser_internal.h"

#include "memorypool.h"

namespace repl
{
	struct State
	{
		State()
		{
			this->topLevelAst = util::pool<ast::TopLevelBlock>(Location(), "__repl");
		}

		ast::TopLevelBlock* topLevelAst;
	};

	static State* state = 0;
	void setupEnvironment()
	{
		state = new State();
	}

	bool processLine(const std::string& line)
	{
		std::string replName = "<repl>";

		frontend::CollectorState collector;

		// lex.
		platform::cachePreExistingFile(replName, line);
		auto lexResult = frontend::lexTokensFromString(replName, line);

		// parse, but first setup the environment.
		auto st = parser::State(lexResult.tokens);
		auto stmt = parser::parseStmt(st, /* exprs: */ true);

		if(stmt.needsMoreTokens())
		{
			return true;
		}
		else if(stmt.isError())
		{
			stmt.err()->post();
		}
		else
		{
			state->topLevelAst->statements.push_back(stmt.val());
		}

		return false;
	}
}















