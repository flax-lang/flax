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




std::string sysroot;
std::string getSysroot()
{
	return sysroot;
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
					sysroot = parseQuotedString(argv, i);
					continue;
				}
				else
				{
					fprintf(stderr, "Error: Expected directory name after '-sysroot' option");
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
					fprintf(stderr, "Error: Expected filename name after '-o' option");
					exit(1);
				}
			}
			else if(!strcmp(argv[i], "-lib"))
			{
				isLib = true;
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

		delete cgi;
	}
	else
	{
		fprintf(stderr, "Expected at least one argument\n");
		exit(-1);
	}
}





