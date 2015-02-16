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
	static bool dumpModule = false;
	static bool compileOnly = false;
	bool getIsCompileOnly()
	{
		return compileOnly;
	}

	static std::string sysroot;
	std::string getSysroot()
	{
		return sysroot;
	}

	static std::string target;
	std::string getTarget()
	{
		return target;
	}

	static int optLevel;
	int getOptimisationLevel()
	{
		return optLevel;
	}

	static uint64_t Flags;
	bool getFlag(Flag f)
	{
		return (Flags & (uint64_t) f);
	}

	static bool isPIC = false;
	bool getIsPositionIndependent()
	{
		return isPIC;
	}

	static std::string mcmodel;
	std::string getMcModel()
	{
		return mcmodel;
	}
}

int main(int argc, char* argv[])
{
	if(argc > 1)
	{
		// parse arguments
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
			if(!strcmp(argv[i], "-target"))
			{
				if(i != argc - 1)
				{
					i++;
					Compiler::target = parseQuotedString(argv, i);
					continue;
				}
				else
				{
					fprintf(stderr, "Error: Expected target string after '-target' option\n");
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
			else if(!strcmp(argv[i], "-fPIC"))
			{
				Compiler::isPIC = true;
			}
			else if(!strcmp(argv[i], "-mcmodel"))
			{
				if(i != argc - 1)
				{
					i++;
					std::string mm = parseQuotedString(argv, i);
					if(mm != "kernel" && mm != "small" && mm != "medium" && mm != "large")
					{
						fprintf(stderr, "Error: valid options for '-mcmodel' are 'small', 'medium', 'large' and 'kernel'. '%s' is invalid.\n", mm.c_str());
						exit(1);
					}

					Compiler::mcmodel = mm;
				}
				else
				{
					fprintf(stderr, "Error: Expected mcmodel name after '-mcmodel' option\n");
					exit(1);
				}
			}
			else if(!strcmp(argv[i], "-Werror"))
			{
				Compiler::Flags |= (uint64_t) Compiler::Flag::WarningsAsErrors;
			}
			else if(!strcmp(argv[i], "-w"))
			{
				Compiler::Flags |= (uint64_t) Compiler::Flag::NoWarnings;
			}
			else if(!strcmp(argv[i], "-dump-ir"))
			{
				Compiler::dumpModule = true;
			}
			else if(!strcmp(argv[i], "-c"))
			{
				Compiler::compileOnly = true;
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
		Root* r = Compiler::compileFile(filename, filelist, rootmap, cgi);

		std::string foldername;
		size_t sep = filename.find_last_of("\\/");
		if(sep != std::string::npos)
			foldername = filename.substr(0, sep);

		Compiler::compileProgram(cgi, filelist, foldername, outname);

		// clean up the intermediate files (ie. .bitcode files)
		for(auto s : filelist)
			remove(s.c_str());

		if(Compiler::dumpModule)
			cgi->mainModule->dump();

		delete cgi;
		for(auto p : rootmap)
			delete p.second;

		delete r;
	}
	else
	{
		fprintf(stderr, "Expected at least one argument\n");
		exit(-1);
	}
}





