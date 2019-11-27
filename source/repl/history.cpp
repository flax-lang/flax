// history.cpp
// Copyright (c) 2019, zhiayang
// Licensed under the Apache License Version 2.0.

#include <errno.h>
#include <string.h>

#include <fstream>

#include "repl.h"
#include "platform.h"

namespace repl
{
	static std::string getConfigPath()
	{
		auto home = platform::getEnvironmentVar("HOME");

		// do some checks so we don't try to write stuff into the root directory.
		return (home + (home.empty() || home.back() == '/' ? "" : "/")) + ".flax-repl-history";
	}


	void saveHistory(const std::vector<std::vector<std::string>>& history)
	{
		auto path = getConfigPath();
		auto file = std::ofstream(path, std::ios::out | std::ios::binary | std::ios::trunc);
		if(!file.is_open() || !file.good())
		{
			repl::log("failed to open file to load history (tried '%s')", path);

			char buf[128] = { 0 };
		#if OS_WINDOWS
			strerror_s(buf, 127, errno);
		#else
			strerror_r(errno, buf, 127);
		#endif
			repl::log("error was: '%s'", buf);
			return;
		}


		// the format is that each "entry" is NULL-terminated, while each line in each entry is just
		// newline-terminated.

		for(const auto& lines : history)
		{
			for(const auto& line : lines)
			{
				file.write(line.c_str(), line.size());
				file.put('\n');
			}

			// write the null-terminator
			file.put('\0');
		}

		// ok.
		file.close();
	}

	std::vector<std::vector<std::string>> loadHistory()
	{
		auto path = getConfigPath();
		auto file = std::ifstream(path, std::ios::in | std::ios::binary);
		if(!file.is_open() || !file.good())
			return { };

		std::vector<std::vector<std::string>> history;

		while(file.good())
		{
			std::vector<std::string> current;
			for(std::string line; std::getline(file, line, '\n'); )
			{
				current.push_back(line);
				if(file.peek() == '\0')
				{
					file.get();
					break;
				}
			}

			if(!current.empty())
				history.push_back(current);
		}


		file.close();
		return history;
	}
}












