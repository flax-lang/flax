// file.cpp
// Copyright (c) 2014 - 2017, zhiayang
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
	static util::hash_map<std::string, FileInnards> fileList;


	static void getRawLines(const std::string_view& fileContents, bool* crlf, util::FastInsertVector<std::string_view>* rawlines)
	{
		std::string_view view = fileContents;

		bool first = true;
		while(true)
		{
			size_t ln = 0;

			if(first || *crlf)
			{
				ln = view.find("\r\n");
				if(ln != std::string_view::npos && first)
					*crlf = true;
			}

			if((!first && !*crlf) || (first && !*crlf && ln == std::string_view::npos))
				ln = view.find('\n');

			first = false;

			if(ln != std::string_view::npos)
			{
				new (rawlines->getNextSlotAndIncrement()) std::string_view(view.data(), ln + (*crlf ? 2 : 1));
				view.remove_prefix(ln + (*crlf ? 2 : 1));
			}
			else
			{
				break;
			}
		}

		// account for the case when there's no trailing newline, and we still have some stuff stuck in the view.
		if(!view.empty())
		{
			new (rawlines->getNextSlotAndIncrement()) std::string_view(view.data(), view.length());
		}
	}

	static void tokenise(lexer::TokenList* ts, bool crlf, const std::string_view& fileContents,
		const util::FastInsertVector<std::string_view>& lines, Location* pos, std::vector<size_t>* importIndices)
	{
		size_t curLine = 0;
		size_t curOffset = 0;

		bool flag = true;
		size_t i = 0;

		do {
			auto type = lexer::getNextToken(lines, &curLine, &curOffset, fileContents, *pos,
				ts->getNextSlotAndIncrement(), crlf);

			flag = (type != lexer::TokenType::EndOfFile);

			if(type == lexer::TokenType::Import)
				importIndices->push_back(i);

			else if(type == lexer::TokenType::Invalid)
				error(*pos, "invalid token");

			i++;

		} while(flag);

		(*ts)[ts->size() - 1].loc.len = 0;
	}




	static void lex(FileInnards* innards, bool crlf, Location* pos)
	{
		lexer::TokenList& ts = innards->tokens;
		tokenise(&ts, crlf, innards->fileContents, innards->lines, pos, &innards->importIndices);

		innards->didLex = true;
	}

	FileInnards lexTokensFromString(const std::string& fakename, const std::string_view& fileContents)
	{
		// split into lines
		bool crlf = false;
		util::FastInsertVector<std::string_view> rawlines;

		getRawLines(fileContents, &crlf, &rawlines);

		Location pos;
		FileInnards innards;
		{
			pos.fileID = getFileIDFromFilename(fakename);

			innards.fileContents = fileContents;
			innards.lines = std::move(rawlines);
		}

		lex(&innards, crlf, &pos);
		return innards;
	}

	static FileInnards& readFileIfNecessary(const std::string& fullPath)
	{
		// break early if we can
		{
			auto it = fileList.find(fullPath);
			if(it != fileList.end())
				return it->second;
		}

		std::string_view fileContents = platform::readEntireFile(fullPath);


		// split into lines
		bool crlf = false;
		util::FastInsertVector<std::string_view> rawlines;
		getRawLines(fileContents, &crlf, &rawlines);

		Location pos;
		FileInnards& innards = fileList[fullPath];
		{
			pos.fileID = getFileIDFromFilename(fullPath);

			innards.fileContents = fileContents;
			innards.lines = std::move(rawlines);
		}

		lex(&innards, crlf, &pos);
		return innards;
	}

	FileInnards& getFileState(const std::string& name)
	{
		return readFileIfNecessary(name);
	}

	lexer::TokenList& getFileTokens(const std::string& fullPath)
	{
		return readFileIfNecessary(fullPath).tokens;
	}

	std::string getFileContents(const std::string& fullPath)
	{
		return std::string(readFileIfNecessary(fullPath).fileContents);
	}


	static std::vector<std::string> fileNames { "null" };
	static util::hash_map<std::string, size_t> existingNames;
	void cachePreExistingFilename(const std::string& name)
	{
		fileNames.push_back(name);
	}

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

	const util::FastInsertVector<std::string_view>& getFileLines(size_t id)
	{
		return readFileIfNecessary(getFilenameFromID(id)).lines;
	}

	const std::vector<size_t>& getImportTokenLocationsForFile(const std::string& filename)
	{
		return fileList[filename].importIndices;
	}




	std::string getPathFromFile(const std::string& path)
	{
		std::string ret;

		size_t sep = path.find_last_of("\\/");
		if(sep != std::string::npos)
			ret = path.substr(0, sep);

		return ret;
	}

	std::string getFilenameFromPath(const std::string& path)
	{
		std::string ret;

		size_t sep = path.find_last_of("\\/");
		if(sep != std::string::npos)
			ret = path.substr(sep + 1);

		return ret;
	}


	std::string getFullPathOfFile(const std::string& partial)
	{
		std::string full = platform::getFullPath(partial);
		if(full.empty())
			error("nonexistent file %s", partial.c_str());

		return full;
	}

	std::string removeExtensionFromFilename(const std::string& name)
	{
		auto i = name.find_last_of('.');
		return name.substr(0, i);
	}
}













