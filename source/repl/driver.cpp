// driver.cpp
// Copyright (c) 2019, zhiayang
// Licensed under the Apache License Version 2.0.

#include <stdlib.h>

#include "repl.h"
#include "frontend.h"

#define ZTMU_CREATE_IMPL 1
#include "ztmu.h"

#include "replxx/include/replxx.hxx"

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


	void start()
	{
		printf("flax repl -- version %s\n", frontend::getVersion().c_str());
		printf("type :help for help\n\n");

		setupEnvironment();

		std::string input;

		#if 0
		auto st = replxx::Replxx();
		st.bind_key(replxx::Replxx::KEY::ENTER, [&st, &input](char32_t code) -> replxx::Replxx::ACTION_RESULT {
			// try to process the current input.
			using AR = replxx::Replxx::ACTION_RESULT;

			auto inp = std::string(st.get_state().text()) + "\n";

			if(input.empty())
			{
				if(inp.empty())
				{
					return AR::RETURN;
				}
				else if(inp[0] == ':')
				{
					runCommand(inp.substr(1));
					printf("\n");

					return AR::RETURN;
				}
			}

			input += inp;
			if(bool more = processLine(input); more)
			{
				// get more...
				st.print("\n");
				st.print(CONTINUATION_PROMPT_STRING);
				return AR::CONTINUE;
			}
			else
			{
				// ok done.
				return AR::RETURN;
			}
		});

		while(auto line = st.input(PROMPT_STRING))
		{
			// ok, we're done -- clear.
			input.clear();
		}

		#else

		auto st = ztmu::State();
		st.setPrompt(PROMPT_STRING);
		st.setWrappedPrompt(WRAP_PROMPT_STRING);

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
			else if(bool needmore = processLine(input += "\n"); needmore)
			{
				// read more.
				while(auto line = st.readContinuation())
				{
					input += std::string(*line) + "\n";

					needmore = processLine(input);

					if(!needmore)
						break;
				}
			}

			// ok, we're done -- clear.
			input.clear();
		}
		#endif
	}
}

// std::wcerr << L"<writing " << (wchar_t*) _display.data() << L">";

















