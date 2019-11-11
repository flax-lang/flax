// driver.cpp
// Copyright (c) 2019, zhiayang
// Licensed under the Apache License Version 2.0.

#include <stdlib.h>

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
	static constexpr const char* WRAP_PROMPT_STRING         = COLOUR_GREY_BOLD " |" COLOUR_RESET " ";
	static constexpr const char* CONTINUATION_PROMPT_STRING = COLOUR_YELLOW_BOLD ".. " COLOUR_GREY_BOLD ">" COLOUR_RESET " ";

	static constexpr const char* EXTRA_INDENT               = "  ";

	void start()
	{
		printf("flax repl -- version %s\n", frontend::getVersion().c_str());
		printf("type :help for help\n\n");

		setupEnvironment();

		std::string input;

		auto st = ztmu::State();
		st.setPrompt(PROMPT_STRING);
		st.setWrappedPrompt(WRAP_PROMPT_STRING);
		st.setContPrompt(CONTINUATION_PROMPT_STRING);

		st.setKeyHandler(static_cast<ztmu::Key>('}'), [](ztmu::State* st, ztmu::Key k) -> ztmu::HandlerAction {
			// a bit dirty, but we just do this -- if we can find the indent at the back, then remove it.
			auto line = st->getCurrentLine();
			if(line.size() >= 2 && line.find(EXTRA_INDENT, line.size() - strlen(EXTRA_INDENT)) != -1)
				st->setCurrentLine(line.substr(0, line.size() - 2));

			return ztmu::HandlerAction::CONTINUE;
		});

		while(auto line = st.read())
		{
			input += std::string(*line);

			if(input.empty())
				continue;

			if(input[0] == ':')
			{
				runCommand(input.substr(1));
				printf("\n");
			}
			else if(bool needmore = processLine(input); needmore)
			{
				const char* indent = "";
				switch(input.back())
				{
					case '{': [[fallthrough]];
					case '(': [[fallthrough]];
					case '[': [[fallthrough]];
					case ',':
						indent = EXTRA_INDENT;
				}

				// read more.
				while(auto line = st.readContinuation(indent))
				{
					input += "\n" + std::string(*line);
					needmore = processLine(input);

					if(!needmore)
						break;
				}
			}

			// ok, we're done -- clear.
			input.clear();
		}
	}
}

















