// main.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include <iostream>
#include <fstream>
#include <cassert>

#include "include/ast.h"
#include "include/codegen.h"
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



int main(int argc, char* argv[])
{
	if(argc > 1)
	{
		bool isLib = false;
		std::vector<std::string> filenames;
		std::string sysroot;
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
				filenames.push_back(argv[i]);
				break;
			}
		}


		// open the file.
		for(auto filename : filenames)
		{
			std::ifstream file = std::ifstream(filename);
			std::stringstream stream;

			stream << file.rdbuf();
			std::string str = stream.str();

			// parse
			Root* root = Parser::Parse(filename, str);
			Codegen::doCodegen(filename, root);


			std::string foldername;
			size_t sep = filename.find_last_of("\\/");
			if(sep != std::string::npos)
				foldername = filename.substr(0, sep);

			if(!isLib)
			{
				// compile it by invoking clang on the bitcode
				char* inv = new char[256];
				snprintf(inv, 256, "clang++ -o '%s' -L'%s'", outname.empty() ? Codegen::mainModule->getModuleIdentifier().c_str() : outname.c_str(), (sysroot + "/usr/lib").c_str());

				std::string libs;
				for(Import* imp : root->imports)
					libs += " -lCS_" + imp->module;

				std::string final = inv;
				final += libs + " ";

				final += foldername + "/" + Codegen::mainModule->getModuleIdentifier() + ".bc";
				system(final.c_str());

				delete[] inv;
			}
		}
	}
	else
	{
		fprintf(stderr, "Expected at least one argument\n");
		exit(-1);
	}
}





