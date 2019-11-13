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
		else if(s == "reset")   { zpr::println("resetting environment..."); setupEnvironment(); }
		else if(s == "help")    repl::error("no help implemented. ggwp.");
		else                    repl::error("invalid command '%s'.", s);
	}

	static constexpr const char* PROMPT_STRING      = COLOUR_BLUE " * " COLOUR_GREY_BOLD ">" COLOUR_RESET " ";
	static constexpr const char* WRAP_PROMPT_STRING = COLOUR_GREY_BOLD " |" COLOUR_RESET " ";
	static constexpr const char* CONT_PROMPT_STRING = COLOUR_YELLOW_BOLD ".. " COLOUR_GREY_BOLD ">" COLOUR_RESET " ";

	static constexpr const char* EXTRA_INDENT       = "  ";
	static constexpr size_t EXTRA_INDENT_LEN        = std::char_traits<char>::length(EXTRA_INDENT);

	void start()
	{
		zpr::println("flax repl -- version %s", frontend::getVersion());
		zpr::println("type :help for help\n");

		setupEnvironment();

		auto st = ztmu::State();
		st.setPrompt(PROMPT_STRING);
		st.setContPrompt(CONT_PROMPT_STRING);
		st.setWrappedPrompt(WRAP_PROMPT_STRING);

		// we need to put this up here, so the handler can capture it.
		int indentLevel = 0;

		st.setKeyHandler(static_cast<ztmu::Key>('}'), [&indentLevel](ztmu::State* st, ztmu::Key k) -> ztmu::HandlerAction {
			// a bit dirty, but we just do this -- if we can find the indent at the back, then remove it.
			auto line = st->getCurrentLine();
			if(indentLevel > 0 && line.size() >= 2 && line.find(EXTRA_INDENT, line.size() - EXTRA_INDENT_LEN) != -1)
			{
				st->setCurrentLine(line.substr(0, line.size() - 2));
				indentLevel--;
			}

			return ztmu::HandlerAction::CONTINUE;
		});

		while(auto line = st.read())
		{
			auto input = std::string(*line);

			if(input.empty())
				continue;

			if(input[0] == ':')
			{
				runCommand(input.substr(1));
				printf("\n");
			}
			else if(bool needmore = processLine(input); needmore)
			{
				auto calc_indent = [](char c) -> int {
					switch(c)
					{
						case '{': [[fallthrough]];
						case '(': [[fallthrough]];
						case '[': [[fallthrough]];
						case ',':
							return 1;
					}

					return 0;
				};

				auto join_lines = [](const std::vector<std::string>& lines) -> std::string {
					std::string ret;
					for(const auto& l : lines)
						ret += "\n" + l;

					return ret;
				};

				indentLevel = calc_indent(input.back());

				// read more.
				while(auto lines = st.readContinuation(std::string(indentLevel * EXTRA_INDENT_LEN, ' ')))
				{
					auto input = join_lines(*lines);
					indentLevel += calc_indent(input.back());

					needmore = processLine(input);

					if(!needmore)
						break;
				}
			}

			st.addPreviousInputToHistory();

			// add an extra line
			printf("\n");
		}
	}
}

















