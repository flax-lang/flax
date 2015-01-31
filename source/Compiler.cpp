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
#include "include/ast.h"
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
		// first check the current directory.
		std::string name = curpath + "/" + imp->module + ".flx";
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
			std::string builtinlib = getSysroot() + "/usr/local/lib/flaxlibs/" + imp->module + ".flx";

			struct stat buffer;
			if(stat (builtinlib.c_str(), &buffer) == 0)
			{
				return builtinlib;
			}
			else
			{
				Parser::parserError("No module or library with the name '%s' could be found", imp->module.c_str());
				return 0;
			}
		}
	}

	Root* compileFile(std::string filename, std::vector<std::string>& list, std::map<std::string, Ast::Root*>& rootmap, Codegen::CodegenInstance* cgi)
	{
		std::string curpath;
		{
			size_t sep = filename.find_last_of("\\/");
			if(sep != std::string::npos)
				curpath = filename.substr(0, sep);
		}


		std::ifstream file(filename);
		std::stringstream stream;

		stream << file.rdbuf();
		std::string str = stream.str();
		file.close();

		// parse
		Root* root = Parser::Parse(filename, str, cgi);

		// get imports
		for(Import* imp : root->imports)
		{
			std::string fname = resolveImport(imp, curpath);

			// simple, compile the source
			Root* r = nullptr;
			if(rootmap.find(imp->module) != rootmap.end())
			{
				r = rootmap[imp->module];
			}
			else
			{
				Codegen::CodegenInstance* rcgi = new Codegen::CodegenInstance();
				r = compileFile(fname, list, rootmap, rcgi);
				rootmap[imp->module] = r;
				delete rcgi;
			}

			for(auto v : r->publicFuncs)
				root->externalFuncs.push_back(std::pair<FuncDecl*, llvm::Function*>(v.first, v.second));

			for(auto v : r->publicTypes)
				root->externalTypes.push_back(std::pair<Struct*, llvm::Type*>(v.first, v.second));
		}

		Codegen::doCodegen(filename, root, cgi);


		// cgi->mainModule->dump();
		// printf("=============================================\n");


		llvm::verifyModule(*cgi->mainModule, &llvm::errs());
		Codegen::writeBitcode(filename, cgi);

		size_t lastdot = filename.find_last_of(".");
		std::string oname = (lastdot == std::string::npos ? filename : filename.substr(0, lastdot));
		oname += ".bc";

		list.push_back(oname);


		return root;
	}





	void compileProgram(Codegen::CodegenInstance* cgi, std::vector<std::string> filelist, std::string foldername, std::string outname)
	{
		// compile it by invoking clang on the bitcode
		char* inv = new char[1024];
		snprintf(inv, 1024, "clang++ -o '%s' ", outname.empty() ? (foldername + "/" + cgi->mainModule->getModuleIdentifier()).c_str() : outname.c_str());

		std::string final = inv + std::string(" ");
		for(auto s : filelist)
			final += "'" + s + "' ";

		// todo: clang bug, http://clang.llvm.org/doxygen/CodeGenAction_8cpp_source.html:714
		// that warning is not affected by any flags I can pass
		// besides, LLVM itself should have caught everything.
		final += " &>/dev/null";
		system(final.c_str());
		delete[] inv;
	}


	// void compileLibrary(Codegen::CodegenInstance* cgi, std::vector<std::string> filelist, Ast::Root* root, std::string foldername, std::string outname)
	// {
	// 	std::string libname = cgi->mainModule->getModuleIdentifier();
	// 	libname = "libCS_" + libname;

	// 	// create a folder with '.lib'
	// 	int err = mkdir(((outname.empty() ? foldername : outname) + "/" + libname + ".lib").c_str(), 511);	// fuck octal
	// 	if(err == -1 && errno != EEXIST)
	// 		error("Something happened while creating the output library - errno is %d", errno);

	// 	if(!outname.empty() && outname.back() != '/')
	// 		outname.push_back('/');

	// 	if(foldername.back() != '/')
	// 		foldername.push_back('/');

	// 	std::string cxx_invoke = "clang++ -w -dynamiclib -o '" + (outname.empty() ? foldername : outname) + libname + ".lib/" + libname + ".bc' ";

	// 	for(auto s : filelist)
	// 		cxx_invoke += "'" + s + "' ";

	// 	system(cxx_invoke.c_str());


	// 	// now, with the root node, we need to create the file that will output llvm bitcode
	// 	// declaring the exported functions in the library.
	// 	// note: bodies will be erased.

	// 	// std::string libpubname = (outname.empty() ? foldername : outname) + libname + ".lib/" + libname + ".ll";
	// 	// createLibraryDefs(root, libpubname);
	// }





	// std::pair<std::deque<std::pair<FuncDecl*, llvm::Function*>>, std::deque<std::pair<Struct*, llvm::Type*>>> extractLibraryPublicDefs(std::string filename, Codegen::CodegenInstance* cgi)
	// {
	// 	std::deque<std::pair<Struct*, llvm::Type*>> types;
	// 	std::deque<std::pair<FuncDecl*, llvm::Function*>> funcs;

	// 	llvm::SMDiagnostic err;
	// 	llvm::Module* mod = llvm::ParseIRFile(filename, err, llvm::getGlobalContext());
	// 	assert(mod);

	// 	for(decltype(mod->getFunctionList().begin()) it = mod->getFunctionList().begin(); it != mod->getFunctionList().end(); it++)
	// 	{
	// 		llvm::Function* func = it.getNodePtrUnchecked();
	// 		funcs.push_back(std::pair<FuncDecl*, llvm::Function*>(0, func));
	// 	}

	// 	for(decltype(mod->global_begin()) it = mod->global_begin(); it != mod->global_end(); it++)
	// 	{
	// 		// we always get the ptr element type,
	// 		// because llvm global variables are always pointers.

	// 		llvm::Type* type = it.getNodePtrUnchecked()->getType()->getPointerElementType();
	// 		llvm::Value* gvar = it.getNodePtrUnchecked();

	// 		std::string name = gvar->getName();
	// 		assert(name.find("_varType") == 0);
	// 		name = name.substr(strlen("_varType"));
	// 		assert(name.length() > 0);

	// 		Struct* str = new Struct(name);


	// 		types.push_back(std::pair<Struct*, llvm::Type*>(0, type));
	// 	}

	// 	return std::pair<decltype(funcs), decltype(types)>(funcs, types);
	// }

	// void createLibraryDefs(Root* root, std::string filename)
	// {
	// 	// same as CodegenUtils.cpp:doCodegen, except we skip the optimisation stuff,
	// 	// because 1. we don't want llvm to potentially remove stuff we need, and
	// 	// 2. there isn't any real code, just definitions.

	// 	Codegen::CodegenInstance* cgi = new Codegen::CodegenInstance();
	// 	cgi->mainModule = new llvm::Module(Parser::getModuleName(filename), llvm::getGlobalContext());
	// 	cgi->rootNode = root;

	// 	std::string err;
	// 	cgi->execEngine = llvm::EngineBuilder(cgi->mainModule).setErrorStr(&err).create();

	// 	llvm::FunctionPassManager OurFPM = llvm::FunctionPassManager(cgi->mainModule);
	// 	cgi->Fpm = &OurFPM;
	// 	cgi->pushScope();


	// 	llvm::Module& mod = *cgi->mainModule;
	// 	llvm::IRBuilder<>& builder = cgi->mainBuilder;

	// 	for(std::pair<FuncDecl*, llvm::Function*> pair : root->publicFuncs)
	// 	{
	// 		// FuncDecl* pub = pair.first;
	// 		// ValPtr_t vp = pub->codegen(cgi);

	// 		llvm::Function* f = pair.second;
	// 		assert(f->getType()->getPointerElementType()->isFunctionTy());

	// 		// llvm::Function* f = llvm::cast<llvm::Function>(vp.first);
	// 		f->deleteBody();
	// 		mod.getOrInsertFunction(f->getName(), f->getFunctionType());
	// 	}

	// 	for(std::pair<Struct*, llvm::Type*> pair : root->publicTypes)
	// 	{
	// 		Struct* type = pair.first;

	// 		// this is to force the type to get inserted into the thingy
	// 		type->createType(cgi);
	// 		llvm::StructType* v = llvm::cast<llvm::StructType>(pair.second);
	// 		mod.getOrInsertGlobal("_varType" + type->name, v);

	// 		int memberindex = 0;
	// 		for(VarDecl* member : type->members)
	// 		{
	// 			llvm::NamedMDNode* node = mod.getOrInsertNamedMetadata("__" + type->name + "|" + member->name + "|" +
	// 				std::to_string(memberindex));

	// 			llvm::Value* Elts[] = { llvm::MDString::get(llvm::getGlobalContext(), member->type) };
	// 			node->addOperand(llvm::MDNode::get(llvm::getGlobalContext(), Elts));

	// 			memberindex++;
	// 		}

	// 		for(Func* func : type->funcs)
	// 		{
	// 			printf("struct func %s\n", func->decl->name.c_str());

	// 			llvm::NamedMDNode* node = mod.getOrInsertNamedMetadata("__" + type->name + "|"
	// 				+ cgi->mangleName(func->decl->name, func->decl->params) + "|" + std::to_string(memberindex));

	// 			llvm::Value* Elts[] = { llvm::MDString::get(llvm::getGlobalContext(), func->decl->type) };
	// 			node->addOperand(llvm::MDNode::get(llvm::getGlobalContext(), Elts));

	// 			memberindex++;
	// 		}
	// 	}


	// 	llvm::sys::fs::OpenFlags of = (llvm::sys::fs::OpenFlags) 0;
	// 	llvm::raw_fd_ostream rso(filename.c_str(), err, of);

	// 	mod.print(rso, 0);

	// 	delete cgi;
	// }


}





















