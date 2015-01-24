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

#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/Support/SourceMgr.h"

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
				return free(lname), "$" + lib;

			else
			{
				free(lname);
				std::string builtinlib = getSysroot() + "/usr/lib/" + "libCS_" + imp->module + ".lib";

				struct stat buffer;
				if(stat (builtinlib.c_str(), &buffer) == 0)
					return "$" + builtinlib;

				fprintf(stderr, "Error (%s:%lld): No module or library with the name '%s' could be found.\n", imp->posinfo.file.c_str(), imp->posinfo.line, imp->module.c_str());

				exit(-1);
			}
		}
	}

	std::pair<std::deque<std::pair<FuncDecl*, llvm::Function*>>, std::deque<std::pair<Struct*, llvm::Type*>>> extractLibraryPublicDefs(std::string filename, Codegen::CodegenInstance* cgi)
	{
		std::deque<std::pair<Struct*, llvm::Type*>> types;
		std::deque<std::pair<FuncDecl*, llvm::Function*>> funcs;

		llvm::SMDiagnostic err;
		llvm::Module* mod = llvm::ParseIRFile(filename, err, llvm::getGlobalContext());


		for(decltype(mod->getFunctionList().begin()) it = mod->getFunctionList().begin(); it != mod->getFunctionList().end(); it++)
		{
			llvm::Function* func = it.getNodePtrUnchecked();
			funcs.push_back(std::pair<FuncDecl*, llvm::Function*>(0, func));
		}

		for(decltype(mod->global_begin()) it = mod->global_begin(); it != mod->global_end(); it++)
		{
			// we always get the ptr element type,
			// because llvm global variables are always pointers.

			llvm::Type* type = it.getNodePtrUnchecked()->getType()->getPointerElementType();
			types.push_back(std::pair<Struct*, llvm::Type*>(0, type));
		}

		return std::pair<decltype(funcs), decltype(types)>(funcs, types);
	}

	void createLibraryDefs(Root* root, std::string filename)
	{
		// same as CodegenUtils.cpp:doCodegen, except we skip the optimisation stuff,
		// because 1. we don't want llvm to potentially remove stuff we need, and
		// 2. there isn't any real code, just definitions.

		Codegen::CodegenInstance* cgi = new Codegen::CodegenInstance();
		cgi->mainModule = new llvm::Module(Parser::getModuleName(filename), llvm::getGlobalContext());
		cgi->rootNode = root;

		std::string err;
		cgi->execEngine = llvm::EngineBuilder(cgi->mainModule).setErrorStr(&err).create();

		llvm::FunctionPassManager OurFPM = llvm::FunctionPassManager(cgi->mainModule);
		cgi->Fpm = &OurFPM;
		cgi->pushScope();


		llvm::Module& mod = *cgi->mainModule;
		llvm::IRBuilder<>& builder = cgi->mainBuilder;

		for(std::pair<FuncDecl*, llvm::Function*> pair : root->publicFuncs)
		{
			FuncDecl* pub = pair.first;
			ValPtr_p vp = pub->codegen(cgi);
			assert(vp.first->getType()->getPointerElementType()->isFunctionTy());

			llvm::Function* f = llvm::cast<llvm::Function>(vp.first);
			f->deleteBody();
			mod.getOrInsertFunction(f->getName(), f->getFunctionType());
		}

		for(std::pair<Struct*, llvm::Type*> pair : root->publicTypes)
		{
			Struct* type = pair.first;

			// this is to force the type to get inserted into the thingy
			type->createType(cgi);
			llvm::StructType* v = llvm::cast<llvm::StructType>(cgi->getType(type->name)->first);
			mod.getOrInsertGlobal(("_varType" + type->name), v);

			for(VarDecl* member : type->members)
			{
				llvm::NamedMDNode* node = mod.getOrInsertNamedMetadata("__" + type->name + "|" + member->name);
				llvm::Value* Elts[] = { llvm::MDString::get(llvm::getGlobalContext(), member->type) };

				node->addOperand(llvm::MDNode::get(llvm::getGlobalContext(), Elts));
			}
		}


		llvm::sys::fs::OpenFlags of = (llvm::sys::fs::OpenFlags) 0;
		llvm::raw_fd_ostream rso(filename.c_str(), err, of);

		mod.print(rso, 0);

		delete cgi;
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
		Root* root = Parser::Parse(filename, str, cgi);

		// get imports
		for(Import* imp : root->imports)
		{
			std::string fname = resolveImport(imp);

			if(fname[0] != '$')
			{
				Codegen::CodegenInstance* rcgi = new Codegen::CodegenInstance();
				Root* r = compileFile(fname, list, rcgi);
				delete rcgi;

				for(auto v : r->publicFuncs)
					root->externalFuncs.push_back(std::pair<FuncDecl*, llvm::Function*>(v.first, v.second));

				for(auto v : r->publicTypes)
					root->externalTypes.push_back(std::pair<Struct*, llvm::Type*>(v.first, v.second));
			}
			else
			{
				// TODO:
				// TODO:
				// BIG FUCKING TODO:
				// FUCK ME

				// Fix library importing of types.
				// 1. llvm currently does not store the names of struct members.
				// 2. We need the Ast::Struct* field in the type table to be able to access members by name
				// 3. We need to be able to, obviously, get access to Ast::Struct to be able to access members
				// 4. Fuck.

				// Fix (eventually)
				// Have the library symbol extractor output corescript instead.
				// Functions will still go into the .ll file (because that still works, for functions)
				// Types (structs) will just be output as corescript.
				// possibly need to create a function in codegen to create a bogus 'Ast::Struct' instance...
				// god dammit.


				fname = fname.substr(1);
				std::string lname = fname;

				// extract the library name
				{
					size_t sep = fname.find_last_of("\\/");
					if(sep != std::string::npos)
						lname = fname.substr(sep + 1);

					size_t lastdot = lname.find_last_of(".");
					lname = (lastdot == std::string::npos ? lname : lname.substr(0, lastdot));
					lname += ".ll";
				}

				auto ret = extractLibraryPublicDefs(fname + "/" + lname, cgi);
				for(auto f : ret.first)
					root->externalFuncs.push_back(std::pair<FuncDecl*, llvm::Function*>(f.first, f.second));

				// for(auto t : *ret.second)
				// 	root->externalTypes.push_back(llvm::cast<llvm::StructType>(t));


				root->referencedLibraries.push_back(imp->module);
			}
		}

		Codegen::doCodegen(filename, root, cgi);

		llvm::verifyModule(*cgi->mainModule, &llvm::errs());
		Codegen::writeBitcode(filename, cgi);

		size_t lastdot = filename.find_last_of(".");
		std::string oname = (lastdot == std::string::npos ? filename : filename.substr(0, lastdot));
		oname += ".bc";

		list.push_back(oname);

		return root;
	}





	void compileProgram(Codegen::CodegenInstance* cgi, std::vector<std::string> filelist, Ast::Root* root, std::string foldername, std::string outname)
	{
		// compile it by invoking clang on the bitcode
		char* inv = new char[1024];
		snprintf(inv, 1024, "clang++ -o '%s' ", outname.empty() ? (foldername + "/" + cgi->mainModule->getModuleIdentifier()).c_str() : outname.c_str());

		std::string libs;
		for(std::string lib : root->referencedLibraries)
			libs += "-L'" + getSysroot() + "/usr/lib/libCS_" + lib + ".lib'" + " -lCS_" + lib;

		std::string final = inv;
		final += libs + " ";

		for(auto s : filelist)
			final += "'" + s + "' ";

		system(final.c_str());

		delete[] inv;
	}


	void compileLibrary(Codegen::CodegenInstance* cgi, std::vector<std::string> filelist, Ast::Root* root, std::string foldername, std::string outname)
	{
		std::string libname = cgi->mainModule->getModuleIdentifier();
		libname = "libCS_" + libname;

		// create a folder with '.lib'
		int err = mkdir(((outname.empty() ? foldername : outname) + "/" + libname + ".lib").c_str(), 511);	// fuck octal
		if(err == -1 && errno != EEXIST)
			error("Something happened while creating the output library - errno is %d", errno);

		if(!outname.empty() && outname.back() != '/')
			outname.push_back('/');

		if(foldername.back() != '/')
			foldername.push_back('/');

		std::string cxx_invoke = "clang++ -w -dynamiclib -o '" + (outname.empty() ? foldername : outname) + libname + ".lib/" + libname + ".dylib' ";

		for(auto s : filelist)
			cxx_invoke += "'" + s + "' ";

		system(cxx_invoke.c_str());


		// now, with the root node, we need to create the file that will output llvm bitcode
		// declaring the exported functions in the library.
		// note: bodies will be erased.

		std::string libpubname = (outname.empty() ? foldername : outname) + libname + ".lib/" + libname + ".ll";
		createLibraryDefs(root, libpubname);
	}
}












