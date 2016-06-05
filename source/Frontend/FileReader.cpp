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
	};

	static std::unordered_map<std::string, FileInnards> fileList;

	static void readFile(std::string fullPath)
	{
		std::ifstream fstr(fullPath);

		std::string fileContents;
		if(fstr)
		{
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
		std::string contents;

		{
			std::stringstream ss(fileContents);
			contents = ss.str();

			std::string tmp;
			while(std::getline(ss, tmp, '\n'))
				rawlines.push_back(tmp);
		}

		Parser::Token curtok;
		Parser::Pin pos;
		Parser::TokenList ts;

		pos.file = new char[fullPath.length() + 1];
		strcpy(pos.file, fullPath.c_str());

		while((curtok = getNextToken(fileContents, pos)).text.size() > 0)
			ts.push_back(curtok);

		FileInnards innards;
		innards.tokens = ts;
		innards.lines = rawlines;
		innards.contents = contents;

		fileList[fullPath] = innards;
	}


	Parser::TokenList getFileTokens(std::string fullPath)
	{
		if(fileList.find(fullPath) == fileList.end())
		{
			readFile(fullPath);
			assert(fileList.find(fullPath) != fileList.end());
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









