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
		setupEnvironment();
		linenoiseSetMultiLine(1);

		printf("flax repl -- version %s\n", frontend::getVersion().c_str());
		printf("type :help for help\n\n");

		std::string input;
		while(char* line = linenoise(PROMPT_STRING))
		{
			input += std::string(line) + "\n";
			linenoiseFree(line);

			if(input.empty())
				continue;

			if(input[0] == ':')
			{
				runCommand(input.substr(1));
				printf("\n");
				continue;
			}

			if(bool needmore = processLine(input); needmore)
			{
				// read more.
				while(needmore)
				{
					char* line = linenoise(CONTINUATION_PROMPT_STRING);

					input += std::string(line) + "\n";
					linenoiseFree(line);

					needmore = processLine(input);
				}
			}

			// ok, we're done -- clear.
			input.clear();
		}
	}
}


















