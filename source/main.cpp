// main.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include <iostream>
#include <fstream>
#include <cassert>
#include <sys/stat.h>
#include <sys/types.h>

#include "include/ast.h"
#include "include/codegen.h"
#include "include/compiler.h"
using namespace Ast;

static std::string parseQuotedString(char** argv, int& i)
{
	std::string ret;
	if(strlen(argv[i]) > 0)
	{
		if(argv[i][0] == '"' || argv[i][0] == '\'')
		{
			while(std::string(argv[i]).back() != '\'' && std::string(argv[i]).back() != '"')
			{
				ret += " " + std::string(argv[i]);
				i++;
			}
		}
		else
		{
			ret = argv[i];
		}
	}
	return ret;
}




namespace Compiler
{
	static uint64_t Flags;

	static std::string sysroot;
	std::string getSysroot()
	{
		return sysroot;
	}

	static int optLevel;
	int getOptimisationLevel()
	{
		return optLevel;
	}

	bool getFlag(Flag f)
	{
		return (Flags & (uint64_t) f);
	}
}

int main(int argc, char* argv[])
{
	if(argc > 1)
	{
		// parse arguments
		bool isLib = false;
		std::string filename;
		std::string outname;

		// parse the command line opts
		for(int i = 1; i < argc; i++)
		{
			if(!strcmp(argv[i], "-sysroot"))
			{
				if(i != argc - 1)
				{
					i++;
					Compiler::sysroot = parseQuotedString(argv, i);
					continue;
				}
				else
				{
					fprintf(stderr, "Error: Expected directory name after '-sysroot' option\n");
					exit(1);
				}
			}
			else if(!strcmp(argv[i], "-o"))
			{
				if(i != argc - 1)
				{
					i++;
					outname = parseQuotedString(argv, i);
					continue;
				}
				else
				{
					fprintf(stderr, "Error: Expected filename name after '-o' option\n");
					exit(1);
				}
			}
			else if(!strcmp(argv[i], "-lib"))
			{
				isLib = true;
			}
			else if(!strcmp(argv[i], "-Werror"))
			{
				Compiler::Flags |= (uint64_t) Compiler::Flag::WarningsAsErrors;
			}
			else if(!strcmp(argv[i], "-w"))
			{
				Compiler::Flags |= (uint64_t) Compiler::Flag::NoWarnings;
			}
			else if(strstr(argv[i], "-O") == argv[i])
			{
				// make sure we have at least 3 chars
				if(strlen(argv[i]) < 3)
				{
					fprintf(stderr, "Error: '-O' is not a valid option on its own\n");
					exit(1);
				}

				Compiler::optLevel = argv[i][2] - '0';
			}
			else if(argv[i][0] == '-')
			{
				fprintf(stderr, "Error: Unrecognised option '%s'\n", argv[i]);

				exit(1);
			}
			else
			{
				filename = argv[i];
				break;
			}
		}


		// compile the file.
		// the file Compiler.cpp handles imports.
		std::vector<std::string> filelist;
		std::map<std::string, Ast::Root*> rootmap;

		Codegen::CodegenInstance* cgi = new Codegen::CodegenInstance();
		Root* root = Compiler::compileFile(filename, filelist, rootmap, cgi);

		std::string foldername;
		size_t sep = filename.find_last_of("\\/");
		if(sep != std::string::npos)
			foldername = filename.substr(0, sep);

		Compiler::compileProgram(cgi, filelist, foldername, outname);

		// clean up the intermediate files (ie. .bitcode files)
		for(auto s : filelist)
			remove(s.c_str());

		// cgi->mainModule->dump();
		delete cgi;
	}
	else
	{
		fprintf(stderr, "Expected at least one argument\n");
		exit(-1);
	}
}





