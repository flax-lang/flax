// Compiler.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include <iostream>
#include <fstream>
#include <cassert>
#include <fstream>
#include <cstdlib>
#include <cinttypes>

#include <sys/stat.h>
#include "errors.h"
#include "parser.h"
#include "codegen.h"
#include "compiler.h"
#include "dependency.h"

#define __STDC_LIMIT_MACROS
#define __STDC_CONSTANT_MACROS

#include "llvm/IR/Verifier.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/MemoryBuffer.h"

using namespace Ast;

namespace Compiler
{
	static HighlightOptions prettyErrorImport(Import* imp, std::string fpath)
	{
		HighlightOptions ops;
		ops.caret = imp->pin;
		ops.caret.fileID = getFileIDFromFilename(fpath);

		auto tmp = imp->pin;
		tmp.col += std::string("import ").length() + 1;
		tmp.len = imp->module.length();

		ops.underlines.push_back(tmp);

		return ops;
	}



	std::string resolveImport(Import* imp, std::string fullPath)
	{
		std::string curpath = getPathFromFile(fullPath);

		if(imp->module.find("*") != (size_t) -1)
		{
			parserError(imp->pin, "Wildcard imports are currently not supported (trying to import %s)", imp->module.c_str());
		}

		// first check the current directory.
		std::string modname = imp->module;
		std::replace(modname.begin(), modname.end(), '.', '/');

		std::string name = curpath + "/" + modname + ".flx";
		char* fname = realpath(name.c_str(), 0);

		// a file here
		if(fname != NULL)
		{
			auto ret = std::string(fname);
			free(fname);

			return getFullPathOfFile(ret);
		}
		else
		{
			free(fname);
			std::string builtinlib = getSysroot() + getPrefix() + modname + ".flx";

			struct stat buffer;
			if(stat(builtinlib.c_str(), &buffer) == 0)
			{
				return getFullPathOfFile(builtinlib);
			}
			else
			{
				std::string msg = "No module or library with the name '" + modname + "' could be found (no such builtin library either)";

				va_list ap;

				__error_gen(prettyErrorImport(imp, fullPath), msg.c_str(), "Error", true, ap);
				abort();
			}
		}
	}





	static void cloneCGIInnards(Codegen::CodegenInstance* from, Codegen::CodegenInstance* to)
	{
		to->typeMap					= from->typeMap;
		to->customOperatorMap		= from->customOperatorMap;
		to->customOperatorMapRev	= from->customOperatorMapRev;
		to->globalConstructors		= from->globalConstructors;
	}

	static void copyRootInnards(Codegen::CodegenInstance* cgi, Root* from, Root* to, bool doClone)
	{
		using namespace Codegen;

		// todo: deprecate this shit
		for(auto v : from->typeList)
		{
			for(auto k : to->typeList)
			{
				if(std::get<0>(k) == std::get<0>(v))
					goto skip;
			}

			to->typeList.push_back(v);

			skip:
			;
		}
	}

	static Codegen::CodegenInstance* _compileFile(std::string fpath, Codegen::CodegenInstance* rcgi, Root* dummyRoot)
	{
		auto p = prof::Profile("compileFile");

		using namespace Codegen;
		using namespace Parser;

		CodegenInstance* cgi = new CodegenInstance();
		cloneCGIInnards(rcgi, cgi);

		cgi->customOperatorMap = rcgi->customOperatorMap;
		cgi->customOperatorMapRev = rcgi->customOperatorMapRev;

		std::string curpath = Compiler::getPathFromFile(fpath);

		// parse
		Root* root = Parser::Parse(cgi, fpath);
		cgi->rootNode = root;


		// add the previous stuff to our own root
		copyRootInnards(cgi, dummyRoot, root, true);


		cgi->module = new fir::Module(Parser::getModuleName(fpath));
		cgi->importOtherCgi(rcgi);


		auto q = prof::Profile("codegen");
		Codegen::doCodegen(fpath, root, cgi);
		q.finish();

		// add the new stuff to the main root
		// todo: check for duplicates

		rcgi->customOperatorMap = cgi->customOperatorMap;
		rcgi->customOperatorMapRev = cgi->customOperatorMapRev;

		return cgi;
	}


	static void _resolveImportGraph(Codegen::DependencyGraph* g, std::unordered_map<std::string, bool>& visited, std::string currentMod,
		std::string curpath)
	{
		using namespace Parser;

		// NOTE: make sure resolveImport **DOES NOT** use codegeninstance, cuz it's 0.

		auto q = prof::Profile("getFileTokens");
		ParserState fakeps(0, Compiler::getFileTokens(currentMod));
		q.finish();


		Parser::setStaticState(fakeps);

		auto p = prof::Profile("find imports");

		for(size_t imp : Compiler::getImportTokenLocationsForFile(currentMod))
		{
			fakeps.reset();
			Token t = fakeps.skip(imp);

			iceAssert(t.type == TType::Import);
			{
				Import* imp = parseImport(fakeps);

				std::string file = Compiler::getFullPathOfFile(Compiler::resolveImport(imp, Compiler::getFullPathOfFile(currentMod)));
				g->addModuleDependency(currentMod, file, imp);

				if(!visited[file])
				{
					visited[file] = true;
					_resolveImportGraph(g, visited, file, curpath);
				}
			}
		}

		p.finish();
	}

	static Codegen::DependencyGraph* resolveImportGraph(std::string baseFullPath, std::string curpath)
	{
		using namespace Codegen;
		DependencyGraph* g = new DependencyGraph();

		std::unordered_map<std::string, bool> visited;
		_resolveImportGraph(g, visited, baseFullPath, curpath);

		return g;
	}










	using namespace Codegen;
	std::vector<std::vector<DepNode*>> checkCyclicDependencies(std::string filename)
	{
		filename = getFullPathOfFile(filename);
		std::string curpath = getPathFromFile(filename);

		DependencyGraph* g = resolveImportGraph(filename, curpath);

		// size_t acc = 0;
		// for(auto e : g->edgesFrom)
		// 	acc += e.second.size();

		// printf("%zu edges in graph\n", acc);

		std::vector<std::vector<DepNode*>> groups = g->findCyclicDependencies();

		for(auto gr : groups)
		{
			if(gr.size() > 1)
			{
				std::string modlist;
				std::vector<Expr*> imps;

				for(auto m : gr)
				{
					std::string fn = getFilenameFromPath(m->name);
					fn = fn.substr(0, fn.find_last_of('.'));

					modlist += "    " + fn + "\n";
				}

				info("Cyclic import dependencies between these modules:\n%s", modlist.c_str());
				info("Offending import statements:");

				for(auto m : gr)
				{
					for(auto u : m->users)
					{
						va_list ap;

						__error_gen(prettyErrorImport(dynamic_cast<Import*>(u.second), u.first->name), "here", "Note", false, ap);
					}
				}

				error("Cyclic dependencies found, cannot continue");
			}
		}

		return groups;
	}



	CompiledData compileFile(std::string filename, std::vector<std::vector<DepNode*>> groups, std::map<Ast::ArithmeticOp,
		std::pair<std::string, int>> foundOps, std::map<std::string, Ast::ArithmeticOp> foundOpsRev)
	{
		filename = getFullPathOfFile(filename);

		std::unordered_map<std::string, Root*> rootmap;
		std::vector<std::pair<std::string, fir::Module*>> modulelist;


		Root* dummyRoot = new Root();
		CodegenInstance* rcgi = new CodegenInstance();
		rcgi->rootNode = dummyRoot;
		rcgi->module = new fir::Module("dummy");

		rcgi->customOperatorMap = foundOps;
		rcgi->customOperatorMapRev = foundOpsRev;

		// fprintf(stderr, "%zu groups (%zu)\n", groups.size(), g->nodes.size());

		if(groups.size() == 0)
		{
			DepNode* dn = new DepNode();
			dn->name = filename;
			groups.insert(groups.begin(), { dn });
		}

		for(auto gr : groups)
		{
			iceAssert(gr.size() == 1);
			std::string name = Compiler::getFullPathOfFile(gr.front()->name);

			auto cgi = _compileFile(name, rcgi, dummyRoot);
			rcgi->importOtherCgi(cgi);

			modulelist.push_back({ name, cgi->module });
			rootmap[name] = cgi->rootNode;

			delete cgi;
		}

		CompiledData ret;

		ret.rootNode = rootmap[Compiler::getFullPathOfFile(filename)];
		ret.rootMap = rootmap;
		ret.moduleList = modulelist;

		return ret;
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
}





















