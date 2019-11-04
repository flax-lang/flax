// driver.cpp
// Copyright (c) 2019, zhiayang
// Licensed under the Apache License Version 2.0.

#include "repl.h"
#include "frontend.h"

#include "linenoise/linenoise.h"

namespace repl
{
	static void runCommand(const std::string& s)
	{
		if(s == "q")            exit(0);
		else if(s == "help")    repl::error("no help implemented. ggwp.");
		else                    repl::error("invalid command '%s'.", s);
	}

	static constexpr const char* PROMPT_STRING              = COLOUR_BLUE " * " COLOUR_GREY_BOLD ">" COLOUR_RESET " ";
	static constexpr const char* CONTINUATION_PROMPT_STRING = COLOUR_YELLOW_BOLD ".. " COLOUR_GREY_BOLD ">" COLOUR_RESET " ";

	void start()
	{
		printf("flax repl -- version %s\n", frontend::getVersion().c_str());
		printf("type :help for help\n\n");

		linenoiseSetMultiLine(1);
		while(char* _line = linenoise(PROMPT_STRING))
		{
			std::string line = _line;
			linenoiseFree(_line);

			if(line.empty())
				continue;

			if(line[0] == ':')
			{
				runCommand(line.substr(1));
				printf("\n");
				continue;
			}

			processLine(line);
			printf("\n");
		}
	}
}


















