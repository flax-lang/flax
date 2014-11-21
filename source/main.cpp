// main.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include <iostream>
#include <fstream>
#include <cassert>

#include "include/ast.h"
#include "include/codegen.h"
using namespace Ast;


int main(int argc, char* argv[])
{
	if(argc > 1)
	{
		std::vector<std::string> filenames;
		std::string sysroot;

		// parse the command line opts
		for(int i = 1; i < argc; i++)
		{
			if(!strcmp(argv[i], "-sysroot"))
			{
				if(i != argc - 1)
				{
					i++;
					sysroot = argv[i];
					continue;
				}
				else
				{
					fprintf(stderr, "Error: Expected directory name after '-sysroot' option");
					exit(1);
				}
			}
			else
			{
				filenames.push_back(argv[i]);
			}
		}




		printf("sysroot = '%s'\n", sysroot.c_str());

		// open the file.
		// for(auto filename : filenames)

		auto filename = filenames.front();
		{
			std::ifstream file = std::ifstream(filename);
			std::stringstream stream;

			stream << file.rdbuf();
			std::string str = stream.str();

			// parse
			Root* root = Parser::Parse(filename, str);
			Codegen::doCodegen(root);



			// compile it by invoking clang on the bitcode
			char* inv = new char[256];
			snprintf(inv, 256, "clang++ -o %s -L%s", Codegen::mainModule->getModuleIdentifier().c_str(), (sysroot + "/usr/lib").c_str());

			std::string libs;
			for(Import* imp : root->imports)
				libs += " -lCS_" + imp->module;

			std::string final = inv;
			final += libs + " ";

			final += Codegen::mainModule->getModuleIdentifier() + ".bc";

			system(final.c_str());
		}
	}
	else
	{
		fprintf(stderr, "Expected at least one argument\n");
	}
}





