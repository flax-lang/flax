// driver.cpp
// Copyright (c) 2019, zhiayang
// Licensed under the Apache License Version 2.0.

#include <unistd.h>

#include "repl.h"
#include "frontend.h"

#define ZTMU_CREATE_IMPL 1
#include "ztmu.h"

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

		setupEnvironment();

		std::string input;

		auto st = ztmu::State();
		st.setPrompt(PROMPT_STRING);
		st.setContinuationPrompt(CONTINUATION_PROMPT_STRING);

		printf("\nread: %s\n", st.read().c_str());





		// printf("len = %zu\n", ztmu::detail::displayedTextLength(PROMPT_STRING));




		// while(auto line = (PROMPT_STRING))
		// {
		// 	input += std::string(line);

		// 	if(input.empty())
		// 		continue;

		// 	if(input[0] == ':')
		// 	{
		// 		runCommand(input.substr(1));
		// 		printf("\n");
		// 		continue;
		// 	}

		// 	if(bool needmore = processLine(input + "\n"); needmore)
		// 	{
		// 		// read more.
		// 		while(needmore)
		// 		{
		// 			auto line = (CONTINUATION_PROMPT_STRING);
		// 			input += std::string(line) + "\n";

		// 			needmore = processLine(input);
		// 		}
		// 	}

		// 	// ok, we're done -- clear.
		// 	input.clear();
		// }
	}
}

// std::wcerr << L"<writing " << (wchar_t*) _display.data() << L">";

















