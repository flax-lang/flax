// Compiler.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include <iostream>
#include <fstream>
#include <cassert>
#include <fstream>
#include <cstdlib>
#include <cinttypes>

#include <sys/stat.h>
#include "include/parser.h"
#include "include/codegen.h"
#include "include/compiler.h"

#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/Support/SourceMgr.h"

using namespace Ast;

namespace Compiler
{
	static std::string resolveImport(Import* imp, std::string curpath)
	{
		if(imp->module.find("*") != (size_t) -1)
		{
			Parser::parserError("Wildcard imports are currently not supported (trying to import %s)", imp->module.c_str());
		}

		// first check the current directory.
		std::string modname = imp->module;
		for(size_t i = 0; i < modname.length(); i++)
		{
			if(modname[i] == '.')
				modname[i] = '/';
		}

		std::string name = curpath + "/" + modname + ".flx";
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
			std::string builtinlib = getSysroot() + getPrefix() + imp->module + ".flx";

			struct stat buffer;
			if(stat(builtinlib.c_str(), &buffer) == 0)
			{
				return builtinlib;
			}
			else
			{
				Parser::parserError("No module or library with the name '%s' could be found", modname.c_str());
			}
		}
	}

	Root* compileFile(std::string filename, std::vector<std::string>& list, std::map<std::string, Ast::Root*>& rootmap, std::vector<llvm::Module*>& modules, Codegen::CodegenInstance* cgi)
	{
		std::string curpath;
		{
			size_t sep = filename.find_last_of("\\/");
			if(sep != std::string::npos)
				curpath = filename.substr(0, sep);
		}

		std::ifstream file(filename);

		// stream << file.rdbuf();
		// std::string str = stream.str();
		// file.close();

		std::string str;
		if(file)
		{
			std::ostringstream contents;
			contents << file.rdbuf();
			file.close();
			str = contents.str();
		}
		else
		{
			perror("There was an error reading the file");
			exit(-1);
		}

		// parse
		Root* root = Parser::Parse(filename, str, cgi);

		// get imports
		for(Expr* e : root->topLevelExpressions)
		{
			Root* r = nullptr;
			Import* imp = dynamic_cast<Import*>(e);

			if(imp)
			{
				std::string fname = resolveImport(imp, curpath);

				// if already compiled, don't do it again
				if(rootmap.find(imp->module) != rootmap.end())
				{
					r = rootmap[imp->module];
				}
				else
				{
					Codegen::CodegenInstance* rcgi = new Codegen::CodegenInstance();
					r = compileFile(fname, list, rootmap, modules, rcgi);

					modules.push_back(rcgi->module);
					rootmap[imp->module] = r;
					delete rcgi;
				}


				// add to both imported and exported lists
				// for(auto v : r->publicFuncs)
				// {
				// 	root->externalFuncs.push_back(cgi->cloneFunctionTree(v, false));
				// 	root->publicFuncs.push_back(cgi->cloneFunctionTree(v, false));

				// 	root->externalFuncs.push_back(std::pair<FuncDecl*, llvm::Function*>(v.first, v.second));
				// 	root->publicFuncs.push_back(std::pair<FuncDecl*, llvm::Function*>(v.first, v.second));
				// }

				for(auto f : r->publicFuncTree.funcs)
				{
					root->externalFuncTree.funcs.push_back(f);
					root->publicFuncTree.funcs.push_back(f);
				}

				for(auto s : r->publicFuncTree.subs)
				{
					root->externalFuncTree.subs.push_back(cgi->cloneFunctionTree(s, false));
					root->publicFuncTree.subs.push_back(cgi->cloneFunctionTree(s, false));
				}

				for(auto v : r->publicTypes)
				{
					root->externalTypes.push_back(std::pair<Struct*, llvm::Type*>(v.first, v.second));
					root->publicTypes.push_back(std::pair<Struct*, llvm::Type*>(v.first, v.second));
				}
				for(auto v : r->publicGenericFunctions)
				{
					root->externalGenericFunctions.push_back(v);
					root->publicGenericFunctions.push_back(v);
				}
				for(auto v : r->typeList)
				{
					bool skip = false;
					for(auto k : root->typeList)
					{
						if(std::get<0>(k) == std::get<0>(v))
						{
							skip = true;
							break;
						}
					}

					if(skip)
						continue;

					root->typeList.push_back(v);
				}
			}
		}

		Codegen::doCodegen(filename, root, cgi);

		// cgi->module->dump();

		llvm::verifyModule(*cgi->module, &llvm::errs());
		Codegen::writeBitcode(filename, cgi);

		size_t lastdot = filename.find_last_of(".");
		std::string oname = (lastdot == std::string::npos ? filename : filename.substr(0, lastdot));
		oname += ".bc";

		list.push_back(oname);
		return root;
	}





	void compileProgram(Codegen::CodegenInstance* cgi, std::vector<std::string> filelist, std::string foldername, std::string outname)
	{
		std::string tgt;
		if(!getTarget().empty())
			tgt = "-target " + getTarget();


		if(!Compiler::getIsCompileOnly() && !cgi->module->getFunction("main"))
		{
			error(0, "No main() function, a program cannot be compiled.");
		}



		std::string oname = outname.empty() ? (foldername + "/" + cgi->module->getModuleIdentifier()).c_str() : outname.c_str();
		// compile it by invoking clang on the bitcode
		char* inv = new char[1024];
		snprintf(inv, 1024, "llvm-link -o '%s.bc'", oname.c_str());
		std::string llvmlink = inv;
		for(auto s : filelist)
			llvmlink += " '" + s + "'";

		system(llvmlink.c_str());

		memset(inv, 0, 1024);
		{
			int opt = Compiler::getOptimisationLevel();
			const char* optLevel	= (Compiler::getOptimisationLevel() >= 0 ? ("-O" + std::to_string(opt)) : "").c_str();
			const char* mcmodel		= (getMcModel().empty() ? "" : ("-mcmodel=" + getMcModel())).c_str();
			const char* isPic		= (getIsPositionIndependent() ? "-fPIC" : "");
			const char* target		= (tgt).c_str();
			const char* outputMode	= (Compiler::getIsCompileOnly() ? "-c" : "");

			snprintf(inv, 1024, "clang++ -flto %s %s %s %s %s -o '%s' '%s.bc'", optLevel, mcmodel, target, isPic, outputMode, oname.c_str(), oname.c_str());
		}
		std::string final = inv;

		// todo: clang bug, http://clang.llvm.org/doxygen/CodeGenAction_8cpp_source.html:714
		// that warning is not affected by any flags I can pass
		// besides, LLVM itself should have caught everything.

		if(!Compiler::getPrintClangOutput())
			final += " &>/dev/null";

		system(final.c_str());

		remove((oname + ".bc").c_str());
		delete[] inv;
	}
}





















