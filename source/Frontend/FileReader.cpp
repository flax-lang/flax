// FileReader.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include <string>
#include <fstream>
#include <unordered_map>

#include <fcntl.h>
#include <assert.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include "parser.h"
#include "compiler.h"

#define USE_MMAP true

#ifdef __MACH__
#include <mach/vm_statistics.h>
#define EXTRA_MMAP_FLAGS VM_FLAGS_SUPERPAGE_SIZE_2MB
#elif defined(MAP_HUGE_2MB)
#define EXTRA_MMAP_FLAGS MAP_HUGE_2MB
#else
#define EXTRA_MMAP_FLAGS 0
#endif

using string_view = std::experimental::string_view;
using namespace Parser;

namespace Lexer
{
	TType getNextToken(const util::FastVector<string_view>& lines, size_t* line, size_t* offset, const string_view& whole, Pin& pos, Token*);
}

namespace Compiler
{
	struct FileInnards
	{
		TokenList tokens;
		string_view fileContents;
		util::FastVector<string_view> lines;
		std::vector<size_t> importIndices;

		bool didLex = false;
		bool isLexing = false;
	};

	static std::unordered_map<std::string, FileInnards> fileList;

	static void readFile(std::string fullPath)
	{
		string_view fileContents;

		{
			auto p = prof::Profile("read file");

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

			fileContents = string_view(contents, fileLength);
		}




		// split into lines
		util::FastVector<string_view> rawlines;
		{
			auto p = prof::Profile("lines");
			string_view view = fileContents;

			while(true)
			{
				size_t ln = view.find('\n');

				if(ln != string_view::npos)
				{
					new (rawlines.getEmptySlotPtrAndAppend()) string_view(view.data(), ln + 1);
					view.remove_prefix(ln + 1);
				}
				else
				{
					break;
				}
			}

			p.finish();
		}


		Pin pos;
		FileInnards& innards = fileList[fullPath];
		{
			pos.fileID = getFileIDFromFilename(fullPath);

			innards.fileContents = std::move(fileContents);
			innards.lines = std::move(rawlines);
			innards.isLexing = true;
		}


		auto p = prof::Profile("lex");


		TokenList& ts = innards.tokens;
		{

			size_t curLine = 0;
			size_t curOffset = 0;

			bool flag = true;
			size_t i = 0;

			do {
				auto type = Lexer::getNextToken(innards.lines, &curLine, &curOffset, innards.fileContents,
					pos, ts.getEmptySlotPtrAndAppend());

				flag = (type != TType::EndOfFile);


				if(type == TType::Import)
					innards.importIndices.push_back(i);

				else if(type == TType::Invalid)
					parserError(pos, "Invalid token");

				i++;

			} while(flag);
		}

		p.finish();

		innards.didLex = true;
		innards.isLexing = false;



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


	TokenList& getFileTokens(std::string fullPath)
	{
		if(fileList.find(fullPath) == fileList.end() || !fileList.at(fullPath).didLex)
		{
			readFile(fullPath);
			assert(fileList.find(fullPath) != fileList.end());
		}
		else if(fileList.at(fullPath).isLexing)
		{
			error("Cannot get token list of file '%s' while still lexing", fullPath.c_str());
		}

		return fileList.at(fullPath).tokens;
	}

	std::string getFileContents(std::string fullPath)
	{
		if(fileList.find(fullPath) == fileList.end())
		{
			readFile(fullPath);
			assert(fileList.find(fullPath) != fileList.end());
		}

		const auto& in = fileList.at(fullPath);
		return in.fileContents.to_string();
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

	const util::FastVector<string_view>& getFileLines(size_t id)
	{
		std::string fp = getFilenameFromID(id);
		if(fileList.find(fp) == fileList.end())
		{
			readFile(fp);
			assert(fileList.find(fp) != fileList.end());
		}

		return fileList.at(fp).lines;
	}

	const std::vector<size_t>& getImportTokenLocationsForFile(const std::string& filename)
	{
		return fileList.at(filename).importIndices;
	}
}













