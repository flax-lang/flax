// commands.cpp
// Copyright (c) 2019, zhiayang
// Licensed under the Apache License Version 2.0.

#include <sstream>

#include "defs.h"
#include "repl.h"
#include "ztmu.h"

#include "sst.h"
#include "ir/type.h"

namespace repl
{
	static void print_help();
	static void print_type(const std::string& expr);

	bool runCommand(const std::string& s, ztmu::State* consoleState)
	{
		if(s == "q")
		{
			repl::log("exiting repl");
			return true;
		}
		else if(s == "reset")
		{
			repl::setupEnvironment();
			repl::log("environment reset");
		}
		else if(s == "help" || s == "?")
		{
			print_help();
		}
		else if(s.find("t ") == 0)
		{
			print_type(s.substr(2));
		}
		else if(s.find("clear_history") == 0)
		{
			// just loading an empty history will effectively clear the history.
			consoleState->loadHistory({ });
		}
		else
		{
			repl::error("invalid command '%s'", s);
		}

		return false;
	}






	static void print_type(const std::string& line)
	{
		bool needmore = false;
		auto stmt = repl::parseAndTypecheck(line, &needmore);
		if(needmore)
		{
			repl::error("':t' does not support continuations");
		}
		else if(!stmt)
		{
			repl::error("invalid expression");
		}
		else if(auto expr = dcast(sst::Expr, *stmt))
		{
			zpr::println("%s%s%s: %s", COLOUR_GREY_BOLD, line, COLOUR_RESET, expr->type);
		}
		else
		{
			repl::error("'%s' is not an expression", (*stmt)->readableName);
		}
	}



	static std::vector<std::string> helpLines = {
		zpr::sprint(""),
		zpr::sprint("%s*%s overview %s*%s", COLOUR_GREEN_BOLD, COLOUR_RESET, COLOUR_GREEN_BOLD, COLOUR_RESET),
		zpr::sprint("\u203e\u203e\u203e\u203e\u203e\u203e\u203e\u203e\u203e\u203e\u203e\u203e"),
		zpr::sprint("The repl accepts Flax expressions and statements; press enter to evaluate the currently entered"
			" input. If the input was incomplete (eg. ending with a '{'), then the repl will enter a multi-line continuation"
			" mode. In either case, use the standard keybindings (arrow keys, home/end, etc.) to navigate."),
		zpr::sprint(""),
		zpr::sprint("Any definitions (eg. variables, functions) will be treated as if they were declared at global"
			" scope, while expressions and statements (eg. loops, arithmetic) will be treated as if they were"
			" written in a function body."),
		zpr::sprint(""),
		zpr::sprint("Expressions with values (eg. 3 + 1) will be given monotonic identifiers (eg. %s_0%s, %s_1%s) that"
			" can be used like any other identifier in code.", COLOUR_BLUE, COLOUR_RESET, COLOUR_BLUE, COLOUR_RESET),
		zpr::sprint(""),
		zpr::sprint("Commands begin with ':', and modify the state of the repl or perform other meta-actions."),

		zpr::sprint(""),
		zpr::sprint("%s*%s commands %s*%s", COLOUR_GREEN_BOLD, COLOUR_RESET, COLOUR_GREEN_BOLD, COLOUR_RESET),
		zpr::sprint("\u203e\u203e\u203e\u203e\u203e\u203e\u203e\u203e\u203e\u203e\u203e\u203e"),
		zpr::sprint(" :? / :help        -  display help (this listing)"),
		zpr::sprint(" :q                -  quit the repl"),
		zpr::sprint(" :reset            -  reset the environment, discarding all existing definitions"),
		zpr::sprint(" :clear_history    -  clear the history of things"),
		zpr::sprint(" :t <expr>         -  display the type of an expression"),
	};

	static void print_help()
	{
		auto xs = ztmu::prettyFormatTextBlock(helpLines, " ", " ");
		for(const auto& x : xs)
			zpr::println(x);
	}

}






