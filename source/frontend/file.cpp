// file.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include <string>
#include <vector>
#include <fstream>
#include <unordered_map>

#include <fcntl.h>
#include <assert.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include "lexer.h"
#include "errors.h"
#include "frontend.h"

#define USE_MMAP true

#ifdef __MACH__
#include <mach/vm_statistics.h>
#define EXTRA_MMAP_FLAGS VM_FLAGS_SUPERPAGE_SIZE_2MB
#elif defined(MAP_HUGE_2MB)
#define EXTRA_MMAP_FLAGS MAP_HUGE_2MB
#else
#define EXTRA_MMAP_FLAGS 0
#endif

namespace frontend
{
	struct FileInnards
	{
		lexer::TokenList tokens;
		stx::string_view fileContents;
		util::FastVector<stx::string_view> lines;
		std::vector<size_t> importIndices;

		bool didLex = false;
		bool isLexing = false;
	};

	static std::unordered_map<std::string, FileInnards> fileList;

	static FileInnards& readFileIfNecessary(std::string fullPath)
	{
		stx::string_view fileContents;

		// break early if we can
		{
			auto it = fileList.find(fullPath);
			if(it != fileList.end() && it->second.didLex)
				return it->second;
		}


		{
			// auto p = prof::Profile("read file");

			// first, get the size of the file
			struct stat st;
			int ret = stat(fullPath.c_str(), &st);

			if(ret != 0)
			{
				perror("There was an error getting the file size");
				exit(-1);
			}

			size_t fileLength = st.st_size;

			int fd = open(fullPath.c_str(), O_RDONLY);
			if(fd == -1)
			{
				perror("There was an error getting opening the file");
				exit(-1);
			}

			// check if we should mmap
			// explanation: if we have EXTRA_MMAP_FLAGS, then we're getting 2MB pages -- in which case we should probably only do it
			// if we have at least 4mb worth of file.
			// if not, then just 2 * pagesize.
			#define MINIMUM_MMAP_THRESHOLD (EXTRA_MMAP_FLAGS ? (2 * 2 * 1024 * 1024) : 2 * getpagesize())
			#define _

			char* contents = 0;
			if(fileLength >= MINIMUM_MMAP_THRESHOLD && USE_MMAP)
			{
				// ok, do an mmap
				contents = (char*) mmap(0, fileLength, PROT_READ, MAP_PRIVATE | EXTRA_MMAP_FLAGS, fd, 0);
				if(contents == MAP_FAILED)
				{
					perror("There was an error getting reading the file");
					exit(-1);
				}
			}
			else
			{
				// read normally
				contents = new char[fileLength + 1];
				size_t didRead = read(fd, contents, fileLength);
				if(didRead != fileLength)
				{
					perror("There was an error getting reading the file");
					exit(-1);
				}
			}
			close(fd);

			fileContents = stx::string_view(contents, fileLength);
		}




		// split into lines
		util::FastVector<stx::string_view> rawlines;
		{
			// auto p = prof::Profile("lines");
			stx::string_view view = fileContents;

			while(true)
			{
				size_t ln = view.find('\n');

				if(ln != stx::string_view::npos)
				{
					new (rawlines.getEmptySlotPtrAndAppend()) stx::string_view(view.data(), ln + 1);
					view.remove_prefix(ln + 1);
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
		return stx::to_string(readFileIfNecessary(fullPath).fileContents);
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

	const util::FastVector<stx::string_view>& getFileLines(size_t id)
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
		const char* fullpath = realpath(partial.c_str(), 0);
		if(fullpath == 0)
			error("Nonexistent file %s", partial.c_str());

		iceAssert(fullpath);

		std::string ret = fullpath;
		free((void*) fullpath);

		return ret;
	}

	std::string removeExtensionFromFilename(std::string name)
	{
		auto i = name.find_last_of(".");
		return name.substr(0, i);
	}
}













