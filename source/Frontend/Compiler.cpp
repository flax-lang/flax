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
#include "../include/parser.h"
#include "../include/codegen.h"
#include "../include/compiler.h"
#include "../include/dependency.h"

#include "llvm/IR/Verifier.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Bitcode/ReaderWriter.h"

using namespace Ast;

namespace Compiler
{
	std::string resolveImport(Import* imp, std::string curpath)
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
			std::string builtinlib = getSysroot() + getPrefix() + modname + ".flx";

			struct stat buffer;
			if(stat(builtinlib.c_str(), &buffer) == 0)
			{
				return builtinlib;
			}
			else
			{
				Parser::Token t;
				t.posinfo = imp->posinfo;
				Parser::parserError(t, "No module or library with the name '%s' could be found (no builtin library '%s' either)",
					modname.c_str(), builtinlib.c_str());
			}
		}
	}






	static Root* _compileFile(Parser::ParserState& pstate, std::string filename, std::vector<std::string>& list,
		std::map<std::string, Ast::Root*>& rootmap, std::vector<llvm::Module*>& modules)
	{
		std::string curpath = Compiler::getPathFromFile(filename);


		std::string str = Compiler::getFileContents(filename);
		pstate.cgi->rawLines = Compiler::getFileLines(filename);

		// parse
		Root* root = Parser::Parse(pstate, filename, str);

		// get imports
		for(Expr* e : root->topLevelExpressions)
		{
			Import* imp = dynamic_cast<Import*>(e);

			if(imp)
			{
				Root* r = nullptr;
				std::string fname = resolveImport(imp, curpath);

				// if already compiled, don't do it again
				if(rootmap.find(imp->module) != rootmap.end())
				{
					r = rootmap[imp->module];
				}
				else
				{
					Codegen::CodegenInstance* rcgi = new Codegen::CodegenInstance();
					rcgi->customOperatorMap = pstate.cgi->customOperatorMap;
					rcgi->customOperatorMapRev = pstate.cgi->customOperatorMapRev;

					Parser::ParserState newPstate(rcgi);

					r = _compileFile(newPstate, fname, list, rootmap, modules);

					modules.push_back(rcgi->module);
					rootmap[imp->module] = r;

					pstate.cgi->customOperatorMap = rcgi->customOperatorMap;
					pstate.cgi->customOperatorMapRev = rcgi->customOperatorMapRev;
					delete rcgi;
				}

				for(auto f : r->publicFuncTree.funcs)
				{
					root->externalFuncTree.funcs.push_back(f);
					root->publicFuncTree.funcs.push_back(f);
				}




				using namespace Codegen;
				std::function<void (FunctionTree*, FunctionTree*)> addSubs = [&](FunctionTree* root, FunctionTree* sub) {

					for(auto s : root->subs)
					{
						if(s->nsName == sub->nsName)
						{
							// add subs of subs instead.
							for(auto ss : sub->subs)
								addSubs(s, ss);

							return;
						}
					}

					root->subs.push_back(pstate.cgi->cloneFunctionTree(sub, false));
				};

				for(auto s : r->publicFuncTree.subs)
				{
					addSubs(&root->externalFuncTree, s);
					addSubs(&root->publicFuncTree, s);
				}






				for(auto v : r->publicTypes)
				{
					root->externalTypes.push_back(std::pair<StructBase*, llvm::Type*>(v.first, v.second));
					root->publicTypes.push_back(std::pair<StructBase*, llvm::Type*>(v.first, v.second));
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

		iceAssert(pstate.cgi);
		Codegen::doCodegen(filename, root, pstate.cgi);

		llvm::verifyModule(*pstate.cgi->module, &llvm::errs());
		Codegen::writeBitcode(filename, pstate.cgi);

		size_t lastdot = filename.find_last_of(".");
		std::string oname = (lastdot == std::string::npos ? filename : filename.substr(0, lastdot));
		oname += ".bc";

		list.push_back(oname);
		return root;
	}


	static void _resolveImportGraph(Codegen::DependencyGraph* g, std::unordered_map<std::string, bool>& visited, std::string currentMod,
		std::string curpath)
	{
		using namespace Parser;

		// NOTE: make sure resolveImport **DOES NOT** use codegeninstance, cuz it's 0.
		ParserState fakeps(0);
		fakeps.currentPos.file = currentMod;
		fakeps.currentPos.line = 1;
		fakeps.currentPos.col = 1;

		fakeps.tokens = Compiler::getFileTokens(currentMod);
		fakeps.origTokens = fakeps.tokens;

		while(fakeps.tokens.size() > 0)
		{
			Token t = fakeps.front();
			fakeps.pop_front();

			if(t.type == TType::Import)
			{
				// hack: parseImport expects front token to be "import"
				fakeps.tokens.push_front(t);

				Import* imp = parseImport(fakeps);

				std::string file = Compiler::resolveImport(imp, curpath);
				const char* fullpath = realpath(file.c_str(), 0);

				file = fullpath;
				free((void*) fullpath);

				g->addModuleDependency(currentMod, file, imp);

				if(!visited[file])
				{
					visited[file] = true;
					_resolveImportGraph(g, visited, file, curpath);
				}
			}
		}
	}

	static Codegen::DependencyGraph* resolveImportGraph(std::string baseFullPath, std::string curpath)
	{
		using namespace Codegen;
		DependencyGraph* g = new DependencyGraph();

		std::unordered_map<std::string, bool> visited;
		_resolveImportGraph(g, visited, baseFullPath, curpath);

		return g;
	}

	Root* compileFile(Parser::ParserState& pstate, std::string filename, std::vector<std::string>& list,
		std::map<std::string, Ast::Root*>& rootmap, std::vector<llvm::Module*>& modules)
	{
		using namespace Codegen;
		const char* full = realpath(filename.c_str(), 0);
		iceAssert(full);
		filename = full;

		free((void*) full);


		std::string curpath = getPathFromFile(filename);

		DependencyGraph* g = resolveImportGraph(filename, curpath);

		std::deque<std::deque<DepNode*>> groups = g->findCyclicDependencies();
		for(auto gr : groups)
		{
			if(gr.size() > 1)
			{
				std::string modlist;
				std::deque<Expr*> imps;

				for(auto m : gr)
				{
					std::string fn = getFilenameFromPath(m->name);
					fn = fn.substr(0, fn.find_last_of('.'));

					modlist += "\t" + fn + "\n";
				}

				info("Cyclic import dependencies between these modules:\n%s", modlist.c_str());
				info("Offending import statements:");

				for(auto m : gr)
				{
					for(auto u : m->users)
					{
						va_list ap;

						__error_gen(getFileLines(u.first->name), u.second->posinfo.line + 1 /* idk why */, u.second->posinfo.col,
							getFilenameFromPath(u.first->name).c_str(), "", "Note", false, ap);
					}
				}

				error("Cyclic dependencies found, cannot continue");
			}
		}


		return _compileFile(pstate, filename, list, rootmap, modules);
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
}





















