// FileReader.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include <string>
#include <fstream>
#include <unordered_map>

#include <assert.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include "parser.h"
#include "compiler.h"

namespace Lexer
{
	// Parser::Token getNextToken(std::experimental::string_view& stream, Parser::Pin& pos);
	Parser::Token getNextToken(std::vector<std::experimental::string_view>& lines, size_t* line, Parser::Pin& pos);
}


namespace Compiler
{
	struct FileInnards
	{
		Parser::TokenList tokens;
		std::vector<std::experimental::string_view> lines;
		std::string contents;

		bool isLexing = false;
		bool didLex = false;
	};

	static std::unordered_map<std::string, FileInnards> fileList;

	static void readFile(std::string fullPath)
	{
		std::ifstream fstr(fullPath);

		std::string fileContents;
		if(fstr)
		{
			auto p = prof::Profile("read file");

			std::ostringstream contents;
			contents << fstr.rdbuf();
			fstr.close();
			fileContents = contents.str();
		}
		else
		{
			perror("There was an error reading the file");
			exit(-1);
		}


		// split into lines
		std::vector<std::experimental::string_view> rawlines;
		std::vector<size_t> linePos;

		{
			auto p = prof::Profile("lines");
			// std::stringstream ss(fileContents);

			std::experimental::string_view view = fileContents;

			while(true)
			{
				size_t ln = view.find('\n');
				if(ln != std::experimental::string_view::npos)
				{
					linePos.push_back(ln);
					rawlines.push_back(view.substr(0, ln + 1));

					view.remove_prefix(ln + 1);
				}
				else
				{
					break;
				}
			}

			p.finish();
		}


		Parser::Pin pos;
		FileInnards& innards = fileList[fullPath];
		{
			pos.file = getStaticFilename(fullPath);

			innards.lines = std::move(rawlines);
			innards.contents = std::move(fileContents);
			innards.isLexing = true;
		}


		auto p = prof::Profile("lex");
		Parser::TokenList& ts = innards.tokens;

		{
			// copy lines.
			auto lines = innards.lines;

			size_t curLine = 0;
			Parser::Token curtok;

			while((curtok = Lexer::getNextToken(lines, &curLine, pos)).type != Parser::TType::EndOfFile)
				ts.push_back(curtok);
		}


		/*
			agv. performance characteristics:

			using vector<> as the TokenList actually results in a 20% *slowdown* vs. using deque<>.
			weird.

			Also, simply lexing, regardless of the list type, takes about 300ms on the long file
			- for deque<>, push_back takes 300ms total.
			- vector<> takes about 400+ ms
		*/


		/*
			vector (-O0):
			5.00
			5.29
			5.81
			5.47
			5.32
			5.58
			5.49
			5.47
			5.63
			5.55


			deque (-O0):
			4.93
	        4.90
	        4.94
	        4.94
	        5.02
	        5.10
	        4.98
	        4.96
	        4.94
	        5.02
		*/


		p.finish();

		innards.didLex = true;
		innards.isLexing = false;
	}

	std::experimental::string_view getStaticFilename(const std::string &fullPath)
	{
		auto it = fileList.find(fullPath);
		assert(it != fileList.end());
		
		return it->first;
	}


	Parser::TokenList& getFileTokens(std::string fullPath)
	{
		if(fileList.find(fullPath) == fileList.end() || !fileList[fullPath].didLex)
		{
			readFile(fullPath);
			assert(fileList.find(fullPath) != fileList.end());
		}
		else if(fileList[fullPath].isLexing)
		{
			error("Cannot get token list of file '%s' while still lexing", fullPath.c_str());
		}

		return fileList[fullPath].tokens;
	}


	std::vector<std::experimental::string_view> getFileLines(std::string fullPath)
	{
		if(fileList.find(fullPath) == fileList.end())
		{
			readFile(fullPath);
			assert(fileList.find(fullPath) != fileList.end());
		}

		return fileList[fullPath].lines;
	}


	std::string getFileContents(std::string fullPath)
	{
		if(fileList.find(fullPath) == fileList.end())
		{
			readFile(fullPath);
			assert(fileList.find(fullPath) != fileList.end());
		}

		return fileList[fullPath].contents;
	}
}









