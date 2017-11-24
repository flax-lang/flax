// file.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include <string>
#include <vector>
#include <fstream>
#include <unordered_map>

#include <fcntl.h>
#include <assert.h>
#include <sys/stat.h>

#include "lexer.h"
#include "errors.h"
#include "frontend.h"

namespace frontend
{
	struct FileInnards
	{
		lexer::TokenList tokens;
		util::string_view fileContents;
		util::FastVector<util::string_view> lines;
		std::vector<size_t> importIndices;

		bool didLex = false;
		bool isLexing = false;
	};

	static std::unordered_map<std::string, FileInnards> fileList;

	static FileInnards& readFileIfNecessary(std::string fullPath)
	{
		// break early if we can
		{
			auto it = fileList.find(fullPath);
			if(it != fileList.end() && it->second.didLex)
				return it->second;
		}


		util::string_view fileContents = platform::readEntireFile(fullPath);



		// split into lines
		util::FastVector<util::string_view> rawlines;
		{
			// auto p = prof::Profile("lines");
			util::string_view view = fileContents;

			bool first = true;
			bool crlf = false;
			while(true)
			{
				size_t ln = 0;

				if(first || crlf)
				{
					ln = view.find("\r\n");
					if(ln != util::string_view::npos)
						crlf = true;
				}

				if(!first || (first && !crlf))
					ln = view.find('\n');

				first = false;

				if(ln != util::string_view::npos)
				{
					new (rawlines.getEmptySlotPtrAndAppend()) util::string_view(view.data(), ln + (crlf ? 2 : 1));
					view.remove_prefix(ln + (crlf ? 2 : 1));
				}
				else
				{
					break;
				}
			}

			// p.finish();
		}


		Location pos;
		FileInnards& innards = fileList[fullPath];
		{
			pos.fileID = getFileIDFromFilename(fullPath);

			innards.fileContents = std::move(fileContents);
			innards.lines = std::move(rawlines);
			innards.isLexing = true;
		}


		// auto p = prof::Profile("lex");


		lexer::TokenList& ts = innards.tokens;
		{

			size_t curLine = 0;
			size_t curOffset = 0;

			bool flag = true;
			size_t i = 0;

			do {
				auto type = lexer::getNextToken(innards.lines, &curLine, &curOffset, innards.fileContents, pos, ts.getEmptySlotPtrAndAppend());

				flag = (type != lexer::TokenType::EndOfFile);

				if(type == lexer::TokenType::Import)
					innards.importIndices.push_back(i);

				else if(type == lexer::TokenType::Invalid)
					error(pos, "Invalid token");

				// debuglog("found token '%s'\n", ts[ts.size() - 1].text);

				i++;

			} while(flag);
		}

		// p.finish();

		innards.didLex = true;
		innards.isLexing = false;

		return innards;

		/*
			file reading stats:

			~175ms reading with c++
			~20ms with read() -- split lines ~70ms
			~4ms with mmap() -- split lines ~87ms


			lexing stats:
			raw lexing takes up ~20ms
			adding to the vector takes ~65ms

			=> resizing ends up taking up 45ms of time
		*/
	}


	lexer::TokenList& getFileTokens(std::string fullPath)
	{
		return readFileIfNecessary(fullPath).tokens;
	}

	std::string getFileContents(std::string fullPath)
	{
		return util::to_string(readFileIfNecessary(fullPath).fileContents);
	}


	static std::vector<std::string> fileNames { "null" };
	static std::unordered_map<std::string, size_t> existingNames;
	const std::string& getFilenameFromID(size_t fileID)
	{
		iceAssert(fileID > 0 && fileID < fileNames.size());
		return fileNames[fileID];
	}

	size_t getFileIDFromFilename(const std::string& name)
	{
		if(existingNames.find(name) != existingNames.end())
		{
			return existingNames[name];
		}
		else
		{
			fileNames.push_back(name);
			existingNames[name] = fileNames.size() - 1;

			return fileNames.size() - 1;
		}
	}

	const util::FastVector<util::string_view>& getFileLines(size_t id)
	{
		std::string fp = getFilenameFromID(id);
		return readFileIfNecessary(fp).lines;
	}

	const std::vector<size_t>& getImportTokenLocationsForFile(const std::string& filename)
	{
		return fileList[filename].importIndices;
	}




	std::string getPathFromFile(std::string path)
	{
		std::string ret;

		size_t sep = path.find_last_of("\\/");
		if(sep != std::string::npos)
			ret = path.substr(0, sep);

		return ret;
	}

	std::string getFilenameFromPath(std::string path)
	{
		std::string ret;

		size_t sep = path.find_last_of("\\/");
		if(sep != std::string::npos)
			ret = path.substr(sep + 1);

		return ret;
	}


	std::string getFullPathOfFile(std::string partial)
	{
		std::string full = platform::getFullPath(partial);
		if(full.empty())
			error("Nonexistent file %s", partial.c_str());

		return full;
	}

	std::string removeExtensionFromFilename(std::string name)
	{
		auto i = name.find_last_of(".");
		return name.substr(0, i);
	}
}













