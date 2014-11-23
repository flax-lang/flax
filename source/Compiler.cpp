// Compiler.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include <iostream>
#include <fstream>
#include <cassert>
#include <fstream>
#include <cstdlib>

#include <sys/stat.h>
#include "include/ast.h"
#include "include/codegen.h"
#include "include/compiler.h"

using namespace Ast;

extern std::string getSysroot();
namespace Compiler
{
	static std::string curpath;
	static std::string resolveImport(Import* imp)
	{
		// first check the current directory.
		std::string name = curpath + "/" + imp->module + ".crs";
		char* fname = realpath(name.c_str(), 0);

		// a file here
		if(fname != NULL)
		{
			auto ret = std::string(fname);
			free(fname);
			return ret;
		}
		else
		{
			free(fname);

			// not a file, try libs.
			// again, check current dir for libs.
			std::string lib = imp->module + ".lib";
			char* lname = realpath(lib.c_str(), 0);

			if(lname != NULL)
				return free(lname), "lib";

			else
			{
				free(lname);
				std::string builtinlib = getSysroot() + "/usr/lib/" + "libCS_" + imp->module + ".lib";

				struct stat buffer;
				if(stat (name.c_str(), &buffer) == 0)
					return "lib";

				fprintf(stderr, "Error (%s:%lld): No module or library with the name '%s' could be found.\n", imp->posinfo.file.c_str(), imp->posinfo.line, imp->module.c_str());

				exit(-1);
			}
		}
	}

	Root* compileFile(std::string filename, std::vector<std::string>& list, Codegen::CodegenInstance* cgi)
	{
		{
			size_t sep = filename.find_last_of("\\/");
			if(sep != std::string::npos)
				curpath = filename.substr(0, sep);
		}


		std::ifstream file = std::ifstream(filename);
		std::stringstream stream;

		stream << file.rdbuf();
		std::string str = stream.str();
		file.close();

		// parse
		Root* root = Parser::Parse(filename, str);

		// get imports
		for(Import* imp : root->imports)
		{
			std::string fname = resolveImport(imp);

			if(fname != "lib")
			{
				Codegen::CodegenInstance* rcgi = new Codegen::CodegenInstance();
				Root* r = compileFile(fname, list, rcgi);
				delete rcgi;

				for(auto v : r->publicFuncs)
					root->externalFuncs.push_back(v);

				for(auto v : r->publicTypes)
					root->externalTypes.push_back(v);
			}
			else
			{
				root->referencedLibraries.push_back(fname);
			}
		}

		Codegen::doCodegen(filename, root, cgi);

		size_t lastdot = filename.find_last_of(".");
		std::string oname = (lastdot == std::string::npos ? filename : filename.substr(0, lastdot));
		oname += ".bc";

		list.push_back(oname);

		return root;
	}
}












