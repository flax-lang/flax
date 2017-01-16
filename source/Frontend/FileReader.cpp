// FileReader.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include <string>
#include <fstream>
#include <unordered_map>

#include <assert.h>

#include "parser.h"
#include "compiler.h"

namespace Compiler
{
	struct FileInnards
	{
		Parser::TokenList tokens;
		std::vector<std::string> lines;
		std::string contents;

		bool isLexing = false;
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
		std::vector<std::string> rawlines;


		{
			auto p = prof::Profile("lines");
			std::stringstream ss(fileContents);

			std::string tmp;
			while(std::getline(ss, tmp, '\n'))
				rawlines.push_back(tmp);

			p.finish();
		}


		Parser::Pin pos;
		Parser::TokenList ts;
		Parser::Token curtok;
		FileInnards innards;
		{
			auto p = prof::Profile("things");
			pos.file = fullPath;

			innards.lines = rawlines;
			innards.contents = fileContents;
			innards.isLexing = true;


			fileList[fullPath] = innards;
		}

		std::experimental::string_view fileContentsView = fileContents;

		auto p = prof::Profile("lex");
		while((curtok = getNextToken(fileContentsView, pos)).type != Parser::TType::EndOfFile)
			ts.push_back(curtok);

		p.finish();


		{
			auto p = prof::Profile("things2");
			fileList[fullPath].tokens = ts;
			fileList[fullPath].isLexing = false;
		}
	}


	Parser::TokenList getFileTokens(std::string fullPath)
	{
		if(fileList.find(fullPath) == fileList.end())
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


	std::vector<std::string> getFileLines(std::string fullPath)
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









