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
	static constexpr const char* PROMPT_STRING      = COLOUR_BLUE_BOLD " * " COLOUR_GREY_BOLD ">" COLOUR_RESET " ";
	static constexpr const char* WRAP_PROMPT_STRING = COLOUR_GREY_BOLD " |" COLOUR_RESET " ";
	static constexpr const char* CONT_PROMPT_STRING = COLOUR_YELLOW_BOLD ".. " COLOUR_GREY_BOLD ">" COLOUR_RESET " ";

	static constexpr const char* EXTRA_INDENT       = "  ";
	static constexpr size_t EXTRA_INDENT_LEN        = std::char_traits<char>::length(EXTRA_INDENT);

	void start()
	{
		zpr::println("flax repl -- version %s", frontend::getVersion());
		zpr::println("type %s:?%s for help\n", COLOUR_GREEN_BOLD, COLOUR_RESET);

		repl::setupEnvironment();

		auto st = ztmu::State();
		st.setPrompt(PROMPT_STRING);
		st.setContPrompt(CONT_PROMPT_STRING);
		st.setWrappedPrompt(WRAP_PROMPT_STRING);
		st.setMessageOnControlC(zpr::sprint("%s(use %s:q%s to quit)%s", COLOUR_GREY_BOLD, COLOUR_GREEN, COLOUR_GREY_BOLD, COLOUR_RESET));

		// temporary.
		st.enableExitOnEmptyControlC();

		// load the history.
		st.loadHistory(repl::loadHistory());

		// setup the console (on windows, this changes to utf8 codepage.)
		st.setupConsole();

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

			// commands start with ':', but also allow '::' path-prefix.
			if(input[0] == ':' && input.find("::") != 0)
			{
				auto quit = repl::runCommand(input.substr(1), &st);
				if(quit) break;
			}
			else if(bool needmore = repl::processLine(input); needmore)
			{
				size_t last_indented_line = 0;
				auto calc_indent = [&last_indented_line, &st](char c) -> int {

					// note: we use +1 so that last_indented_line is 1-indexed. this
					// entire thing is so we don't get increasing indentation levels if
					// we delete and re-enter on a brace.
					if(last_indented_line == 0 || st.lineIdx + 1 > last_indented_line)
					{
						switch(c)
						{
							case '{': [[fallthrough]];
							case '(': [[fallthrough]];
							case '[': [[fallthrough]];
							case ',': {
								last_indented_line = st.lineIdx + 1;
								return 1;
							}
						}
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

					needmore = repl::processLine(input);

					if(!needmore)
						break;
				}
			}

			st.addPreviousInputToHistory();

			// add an extra line
			printf("\n");
		}

		// save the history.
		repl::saveHistory(st.getHistory());

		// restore the console.
		st.unsetupConsole();
	}
}

















