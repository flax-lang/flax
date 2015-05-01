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

#include "llvm/Linker/Linker.h"

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
	static bool printModule = false;
	static bool compileOnly = false;
	bool getIsCompileOnly()
	{
		return compileOnly;
	}

	#define DEFAULT_PREFIX		"/usr/local/lib/flaxlibs/"
	std::string getPrefix()
	{
		return DEFAULT_PREFIX;
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

	static bool noLowercaseTypes;
	bool getDisableLowercaseBuiltinTypes()
	{
		return noLowercaseTypes;
	}

	static bool printClangOutput;
	bool getPrintClangOutput()
	{
		return printClangOutput;
	}

	static bool runProgramWithJit;
	bool getRunProgramWithJit()
	{
		return runProgramWithJit;
	}

	static bool noAutoGlobalConstructor = false;
	bool getNoAutoGlobalConstructor()
	{
		return noAutoGlobalConstructor;
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
					exit(-1);
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
					exit(-1);
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
					exit(-1);
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
						exit(-1);
					}

					Compiler::mcmodel = mm;
				}
				else
				{
					fprintf(stderr, "Error: Expected mcmodel name after '-mcmodel' option\n");
					exit(-1);
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
			else if(!strcmp(argv[i], "-print-ir"))
			{
				Compiler::printModule = true;
			}
			else if(!strcmp(argv[i], "-no-lowercase-builtin"))
			{
				Compiler::noLowercaseTypes = true;
			}
			else if(!strcmp(argv[i], "-c"))
			{
				Compiler::compileOnly = true;
			}
			else if(!strcmp(argv[i], "-show-clang"))
			{
				Compiler::printClangOutput = true;
			}
			else if(!strcmp(argv[i], "-jit") || !strcmp(argv[i], "-run"))
			{
				Compiler::runProgramWithJit = true;
			}
			else if(!strcmp(argv[i], "-no-auto-gconstr"))
			{
				Compiler::noAutoGlobalConstructor = true;
			}
			else if(strstr(argv[i], "-O") == argv[i])
			{
				// make sure we have at least 3 chars
				if(strlen(argv[i]) < 3)
				{
					fprintf(stderr, "Error: '-O' is not a valid option on its own\n");
					exit(-1);
				}

				if(argv[i][2] == 'x')
				{
					// literally nothing
					Compiler::optLevel = -1;
				}
				else
				{
					Compiler::optLevel = argv[i][2] - '0';
				}
			}
			else if(argv[i][0] == '-')
			{
				fprintf(stderr, "Error: Unrecognised option '%s'\n", argv[i]);

				exit(-1);
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
		std::vector<llvm::Module*> modulelist;
		std::map<std::string, Ast::Root*> rootmap;

		Codegen::CodegenInstance* cgi = new Codegen::CodegenInstance();
		Root* r = Compiler::compileFile(filename, filelist, rootmap, modulelist, cgi);





		// needs to be done first, for the weird constructor fiddling below.
		if(Compiler::runProgramWithJit)
		{
			llvm::Linker linker = llvm::Linker(cgi->mainModule);
			for(auto mod : modulelist)
				linker.linkInModule(mod, nullptr);
		}





		bool needGlobalConstructor = false;
		for(auto pair : rootmap)
		{
			if(pair.second->globalConstructorTrampoline != 0)
			{
				needGlobalConstructor = true;
				break;
			}
		}



		if(needGlobalConstructor)
		{
			std::vector<llvm::Function*> constructors;
			for(auto pair : rootmap)
			{
				if(pair.second->globalConstructorTrampoline != 0)
				{
					llvm::Function* constr = cgi->mainModule->getFunction(pair.second->globalConstructorTrampoline->getName());
					constructors.push_back(constr);
				}
			}

			llvm::FunctionType* ft = llvm::FunctionType::get(llvm::Type::getVoidTy(llvm::getGlobalContext()), false);
			llvm::Function* gconstr = llvm::Function::Create(ft, llvm::GlobalValue::ExternalLinkage,
				"__global_constructor_top_level__", cgi->mainModule);

			llvm::BasicBlock* iblock = llvm::BasicBlock::Create(llvm::getGlobalContext(), "initialiser", gconstr);
			cgi->mainBuilder.SetInsertPoint(iblock);

			for(auto f : constructors)
			{
				cgi->mainBuilder.CreateCall(f);
			}

			cgi->mainBuilder.CreateRetVoid();




			if(!Compiler::getNoAutoGlobalConstructor())
			{
				// insert a call at the beginning of main().
				llvm::Function* mainfunc = cgi->mainModule->getFunction("main");

				llvm::BasicBlock* entry = &mainfunc->getEntryBlock();
				llvm::BasicBlock* f = llvm::BasicBlock::Create(cgi->getContext(), "__main_entry", mainfunc);

				f->moveBefore(entry);
				cgi->mainBuilder.SetInsertPoint(f);
				cgi->mainBuilder.CreateCall(gconstr);
				cgi->mainBuilder.CreateBr(entry);
			}
		}


















		std::string foldername;
		size_t sep = filename.find_last_of("\\/");
		if(sep != std::string::npos)
			foldername = filename.substr(0, sep);



		if(Compiler::runProgramWithJit)
		{
			// all linked already.
			// dump here, before the output.
			if(Compiler::printModule)
				cgi->mainModule->dump();

			cgi->execEngine = llvm::EngineBuilder(cgi->mainModule).create();
			if(llvm::Function* mainptr = cgi->mainModule->getFunction("main"))
			{
				void* func = cgi->execEngine->getPointerToFunction(mainptr);
				auto mainfunc = (int (*)(int, const char**)) func;

				const char* m[] = { ("__llvmJIT_" + cgi->mainModule->getModuleIdentifier()).c_str() };
				mainfunc(1, m);
			}
		}
		else
		{
			Compiler::compileProgram(cgi, filelist, foldername, outname);
		}


		// clean up the intermediate files (ie. .bitcode files)
		for(auto s : filelist)
			remove(s.c_str());

		if(Compiler::printModule && !Compiler::getRunProgramWithJit())
			cgi->mainModule->dump();

		if(Compiler::dumpModule)
		{
			// std::string err_info;
			// llvm::raw_fd_ostream out((outname + ".ir").c_str(), err_info, llvm::sys::fs::OpenFlags::F_None);

			// out << *(cgi->mainModule);
			// out.close();

			fprintf(stderr, "enosup\n");
			exit(-1);
		}

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





