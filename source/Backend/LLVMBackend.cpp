// LLVMBackend.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.



#ifndef __STDC_CONSTANT_MACROS
#define __STDC_CONSTANT_MACROS
#endif

#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS
#endif

#include <deque>
#include <vector>
#include <fstream>

#include "llvm/IR/Verifier.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Support/Host.h"
#include "llvm/Linker/Linker.h"
#include "llvm/Transforms/IPO.h"
#include "llvm/Analysis/Passes.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/ExecutionEngine/MCJIT.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/ToolOutputFile.h"
#include "llvm/Support/DynamicLibrary.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"

// #include "llvm/IRReader/IRReader.h"
// #include "llvm/Transforms/Utils/Cloning.h"
// #include "llvm/Transforms/Instrumentation.h"
// #include "llvm/CodeGen/MIRParser/MIRParser.h"
// #include "llvm/ExecutionEngine/SectionMemoryManager.h"

#include "ir/type.h"
#include "ir/value.h"
#include "ir/module.h"
#include "ir/irbuilder.h"

#include "backend.h"
#include "compiler.h"

#include <stdio.h>
#include <spawn.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>



namespace Compiler
{
	LLVMBackend::LLVMBackend(CompiledData& dat, std::deque<std::string> inputs, std::string output) : Backend(BackendCaps::EmitAssembly | BackendCaps::EmitObject | BackendCaps::EmitProgram | BackendCaps::JIT, dat, inputs, output)
	{
		if(inputs.size() != 1)
			_error_and_exit("Need exactly 1 input filename, have %zu", inputs.size());
	}

	std::string LLVMBackend::str()
	{
		return "LLVM";
	}

	void LLVMBackend::performCompilation()
	{
		auto p = prof::Profile(PROFGROUP_LLVM, "llvm_linkmod");

		// this one just does fir -> llvm, then links all the llvm modules together.
		std::unordered_map<std::string, llvm::Module*> modulelist;

		// translate to llvm
		for(auto mod : this->compiledData.moduleList)
			modulelist[mod.first] = mod.second->translateToLlvm();


		auto s = Compiler::getFilenameFromPath(this->inputFilenames[0]);
		s = s.substr(0, s.find_last_of("."));

		llvm::Module* mainModule = new llvm::Module(s, llvm::getGlobalContext());
		llvm::Linker linker = llvm::Linker(*mainModule);

		for(auto mod : modulelist)
			linker.linkInModule(std::unique_ptr<llvm::Module>(mod.second));

		this->linkedModule = mainModule;
		this->finaliseGlobalConstructors();
	}

	void LLVMBackend::optimiseProgram()
	{
		auto p = prof::Profile(PROFGROUP_LLVM, "llvm_optimise");

		llvm::verifyModule(*this->linkedModule);

		llvm::legacy::PassManager fpm = llvm::legacy::PassManager();

		if(Compiler::getOptimisationLevel() > OptimisationLevel::Debug)
		{
			fpm.add(llvm::createPromoteMemoryToRegisterPass());
			fpm.add(llvm::createMergedLoadStoreMotionPass());
			fpm.add(llvm::createScalarReplAggregatesPass());
			fpm.add(llvm::createConstantPropagationPass());
			fpm.add(llvm::createDeadCodeEliminationPass());
			fpm.add(llvm::createLoadCombinePass());
		}

		if(Compiler::getOptimisationLevel() > OptimisationLevel::None)
		{
			// Do simple "peephole" optimisations and bit-twiddling optzns.
			fpm.add(llvm::createInstructionCombiningPass());

			// Reassociate expressions.
			fpm.add(llvm::createReassociatePass());

			// Eliminate Common SubExpressions.
			fpm.add(llvm::createGVNPass());


			// Simplify the control flow graph (deleting unreachable blocks, etc).
			fpm.add(llvm::createCFGSimplificationPass());

			// hmm.
			// fuck it, turn everything on.
			fpm.add(llvm::createConstantHoistingPass());
			fpm.add(llvm::createLICMPass());
			fpm.add(llvm::createDelinearizationPass());
			fpm.add(llvm::createFlattenCFGPass());
			fpm.add(llvm::createScalarizerPass());
			fpm.add(llvm::createSinkingPass());
			fpm.add(llvm::createStructurizeCFGPass());
			fpm.add(llvm::createInstructionSimplifierPass());
			fpm.add(llvm::createDeadStoreEliminationPass());
			fpm.add(llvm::createDeadInstEliminationPass());
			fpm.add(llvm::createMemCpyOptPass());

			fpm.add(llvm::createSCCPPass());

			fpm.add(llvm::createTailCallEliminationPass());
		}

		if(Compiler::getOptimisationLevel() > OptimisationLevel::Minimal)
		{
			fpm.add(llvm::createAggressiveDCEPass());

			// module-level stuff
			fpm.add(llvm::createMergeFunctionsPass());
			fpm.add(llvm::createLoopSimplifyPass());
		}

		iceAssert(this->linkedModule);
		fpm.run(*this->linkedModule);
	}

	void LLVMBackend::writeOutput()
	{
		if(Compiler::getDumpLlvm())
			this->linkedModule->dump();

		// verify the module.
		{
			auto p = prof::Profile(PROFGROUP_LLVM, "llvm_verify");
			llvm::verifyModule(*this->linkedModule, &llvm::errs());
		}

		std::string foldername;
		size_t sep = this->inputFilenames[0].find_last_of("\\/");
		if(sep != std::string::npos)
			foldername = this->inputFilenames[0].substr(0, sep);

		bool wasEmpty = this->outputFilename.empty();
		std::string oname = this->outputFilename.empty() ? (foldername + "/" + this->linkedModule->getModuleIdentifier()).c_str()
			: this->outputFilename.c_str();

		if(Compiler::getOutputMode() == ProgOutputMode::RunJit)
		{
			this->runProgramWithJIT();
		}
		else if(Compiler::getOutputMode() == ProgOutputMode::LLVMBitcode)
		{
			std::error_code e;
			llvm::sys::fs::OpenFlags of = (llvm::sys::fs::OpenFlags) 0;
			llvm::raw_fd_ostream rso(oname.c_str(), e, of);

			llvm::WriteBitcodeToFile(this->linkedModule, rso);
			rso.close();
		}
		else
		{
			auto p = prof::Profile(PROFGROUP_LLVM, "llvm_compile");

			if(Compiler::getOutputMode() != ProgOutputMode::ObjectFile && !this->linkedModule->getFunction("main"))
			{
				fprintf(stderr, "No main() function, a program cannot be compiled\n");
				exit(-1);
			}

			auto buffer = this->initialiseLLVMStuff();

			if(Compiler::getOutputMode() == ProgOutputMode::ObjectFile)
			{
				if(this->linkedModule->getFunction("main") == 0)
				{
					fprintf(stderr, "No main() function, a program cannot be compiled\n");
					exit(-1);
				}

				// now memoryBuffer should contain the .object file
				std::ofstream objectOutput(oname + (wasEmpty ? ".o" : ""), std::ios::binary | std::ios::out);
				objectOutput.write(buffer.data(), buffer.size_in_bytes());
				objectOutput.close();
			}
			else
			{
				char templ[] = "/tmp/fileXXXXXX";
				int fd = mkstemp(templ);


				write(fd, buffer.data(), buffer.size_in_bytes());
				fsync(fd);



				auto libs = Compiler::getLibrariesToLink();
				auto libdirs = Compiler::getLibrarySearchPaths();

				auto frames = Compiler::getFrameworksToLink();
				auto framedirs = Compiler::getFrameworkSearchPaths();

				size_t s = 5 + (2 * libs.size()) + (2 * libdirs.size()) + (2 * frames.size()) + (2 * framedirs.size());
				const char** argv = new const char*[s];
				memset(argv, 0, s * sizeof(const char*));

				argv[0] = "cc";
				argv[1] = "-o";
				argv[2] = oname.c_str();

				size_t i = 3;


				// note: these need to be references
				// if they're not, then the std::string (along with its buffer) is destructed at the end of the loop body
				// so the pointer in argv[i] becomes invalid
				// thus we need to make sure the pointed thing is valid until we call execvp; the frames/libs/blabla deques up there
				// will live for the required duration, so we use a reference.

				for(auto& F : framedirs)
				{
					argv[i] = "-F";			i++;
					argv[i] = F.c_str();	i++;
				}

				for(auto& f : frames)
				{
					argv[i] = "-framework";	i++;
					argv[i] = f.c_str();	i++;
				}

				for(auto& L : libdirs)
				{
					argv[i] = "-L";			i++;
					argv[i] = L.c_str();	i++;
				}

				for(auto& l : libs)
				{
					argv[i] = "-l";			i++;
					argv[i] = l.c_str();	i++;
				}

				argv[s - 2] = templ;
				argv[s - 1] = 0;


				int outpipe[2];
				iceAssert(pipe(outpipe) == 0);

				std::string output;

				pid_t pid = fork();
				int status = 0;
				if(pid == 0)
				{
					// in child, pid == 0.

					close(outpipe[0]);
					dup2(outpipe[1], 1);

					execvp(argv[0], (char* const*) argv);
					abort();
				}
				else
				{
					// wait for child to finish, then we can continue cleanup

					close(outpipe[1]);
					size_t rd = 0;

					char* buf = new char[66];
					while(true)
					{
						rd = read(outpipe[0], buf, 64);
						if(rd == 0)
							break;

						buf[rd] = 0;
						output += buf;
					}

					close(outpipe[0]);

					int s = 0;

					auto p = prof::Profile(PROFGROUP_LLVM, "wait_childproc");
					waitpid(pid, &s, 0);

					status = WEXITSTATUS(s);
				}

				// delete the temp file
				std::remove(templ);

				delete[] argv;

				if(status != 0 || output.size() != 0)
				{
					fprintf(stderr, "%s\n", output.c_str());
					fprintf(stderr, "linker returned non-zero (status = %d), exiting\n", status);
					exit(status);
				}
			}
		}


		// cleanup
		for(auto p : this->compiledData.rootMap)
			delete p.second;
	}
















	llvm::SmallVector<char, 0> LLVMBackend::initialiseLLVMStuff()
	{
		llvm::InitializeAllTargets();
		llvm::InitializeAllTargetMCs();
		llvm::InitializeAllAsmParsers();
		llvm::InitializeAllAsmPrinters();

		llvm::PassRegistry* Registry = llvm::PassRegistry::getPassRegistry();
		llvm::initializeCore(*Registry);
		llvm::initializeCodeGen(*Registry);

		llvm::Triple targetTriple;
		targetTriple.setTriple(Compiler::getTarget().empty() ? llvm::sys::getDefaultTargetTriple() : Compiler::getTarget());



		std::string err_str;
		const llvm::Target* theTarget = llvm::TargetRegistry::lookupTarget("", targetTriple, err_str);
		if(!theTarget)
		{
			fprintf(stderr, "error creating target: (wanted: '%s');\n"
					"llvm error: %s\n", targetTriple.str().c_str(), err_str.c_str());
			exit(-1);
		}


		// get the mcmodel
		llvm::CodeModel::Model codeModel;
		if(Compiler::getCodeModel() == "kernel")
		{
			codeModel = llvm::CodeModel::Kernel;
		}
		else if(Compiler::getCodeModel() == "small")
		{
			codeModel = llvm::CodeModel::Small;
		}
		else if(Compiler::getCodeModel() == "medium")
		{
			codeModel = llvm::CodeModel::Medium;
		}
		else if(Compiler::getCodeModel() == "large")
		{
			codeModel = llvm::CodeModel::Large;
		}
		else if(Compiler::getCodeModel().empty())
		{
			codeModel = llvm::CodeModel::Default;
		}
		else
		{
			fprintf(stderr, "Invalid mcmodel '%s' (valid options: kernel, small, medium, or large)\n",
				Compiler::getCodeModel().c_str());

			exit(-1);
		}


		llvm::TargetOptions targetOptions;

		if(Compiler::getIsPositionIndependent())
			targetOptions.PositionIndependentExecutable = true;


		std::unique_ptr<llvm::TargetMachine> targetMachine(theTarget->createTargetMachine(targetTriple.getTriple(), "", "",
			targetOptions, llvm::Reloc::Default, codeModel, llvm::CodeGenOpt::Default));


		this->linkedModule->setDataLayout(targetMachine->createDataLayout());

		llvm::SmallVector<char, 0> memoryBuffer;
		auto bufferStream = llvm::make_unique<llvm::raw_svector_ostream>(memoryBuffer);
		llvm::raw_pwrite_stream* rawStream = bufferStream.get();

		{
			auto p = prof::Profile(PROFGROUP_LLVM, "llvm_emit_object");
			llvm::legacy::PassManager pm = llvm::legacy::PassManager();
			targetMachine->addPassesToEmitFile(pm, *rawStream, llvm::TargetMachine::CodeGenFileType::CGFT_ObjectFile);
			pm.run(*this->linkedModule);
		}

		// flush and kill it.
		rawStream->flush();

		return memoryBuffer;
	}


















	void LLVMBackend::runProgramWithJIT()
	{
		// all linked already.
		// dump here, before the output.


		// if JIT-ing, we need to load all the framework shit.
		// note(compat): i think -L -l ordering matters when resolving libraries
		// we don't support that right now.
		// now, just add all the paths immediately.

		std::string penv;
		std::string pfenv;

		char* e = std::getenv("LD_LIBRARY_PATH");

		std::string env;
		if(e) env = std::string(e);

		penv = env;

		if(!env.empty() && env.back() != ':')
			env += ":";

		// add all the paths.
		{
			for(auto L : Compiler::getLibrarySearchPaths())
				env += L + ":";

			if(!env.empty() && env.back() == ':')
				env.pop_back();
		}



		std::string fenv;

		char* fe = std::getenv("DYLD_FRAMEWORK_PATH");
		if(fe) fenv = std::string(fe);

		pfenv = fenv;

		if(!fenv.empty() && fenv.back() != ':')
			fenv += ":";

		// add framework paths
		{
			for(auto F : Compiler::getFrameworkSearchPaths())
				fenv += F + ":";

			if(!fenv.empty() && fenv.back() == ':')
				fenv.pop_back();
		}


		// set the things
		setenv("LD_LIBRARY_PATH", env.c_str(), 1);
		setenv("DYLD_FRAMEWORK_PATH", fenv.c_str(), 1);



		for(auto l : Compiler::getLibrariesToLink())
		{
			std::string ext;

			#if defined(__MACH__)
				ext = ".dylib";
			#elif defined(WIN32)
				ext = ".dll";
			#else
				ext = ".so";
			#endif

			std::string err;
			llvm::sys::DynamicLibrary dl = llvm::sys::DynamicLibrary::getPermanentLibrary(("lib" + l + ext).c_str(), &err);
			if(!dl.isValid())
			{
				fprintf(stderr, "Failed to load library '%s', dlopen failed with error:\n%s\n", l.c_str(), err.c_str());
				exit(-1);
			}
		}


		for(auto l : Compiler::getFrameworksToLink())
		{
			std::string name = l + ".framework/" + l;

			std::string err;
			llvm::sys::DynamicLibrary dl = llvm::sys::DynamicLibrary::getPermanentLibrary(name.c_str(), &err);
			if(!dl.isValid())
			{
				fprintf(stderr, "Failed to load framework '%s', dlopen failed with error:\n%s\n", l.c_str(), err.c_str());
				exit(-1);
			}
		}



		if(this->linkedModule->getFunction("main") != 0)
		{
			llvm::ExecutionEngine* execEngine = llvm::EngineBuilder(std::unique_ptr<llvm::Module>(this->linkedModule)).create();
			uint64_t func = execEngine->getFunctionAddress("main");
			iceAssert(func != 0);

			auto mainfunc = (int (*)(int, const char**)) func;

			const char* m[] = { ("__llvmJIT_" + this->linkedModule->getModuleIdentifier()).c_str() };

			// finalise the object, which causes the memory to be executable
			// fucking NX bit
			// ee->finalizeObject();

			mainfunc(1, m);
		}
		else
		{
			_error_and_exit("No main() function, cannot JIT");
		}


		// restore
		setenv("LD_LIBRARY_PATH", penv.c_str(), 1);
		setenv("DYLD_FRAMEWORK_PATH", pfenv.c_str(), 1);
	}















	void LLVMBackend::finaliseGlobalConstructors()
	{
		auto& cd = this->compiledData;

		auto& rootmap = this->compiledData.rootMap;

		bool needGlobalConstructor = false;
		if(cd.rootNode->globalConstructorTrampoline != 0) needGlobalConstructor = true;
		for(auto pair : cd.rootMap)
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
					llvm::Function* constr = this->linkedModule->getFunction(pair.second->globalConstructorTrampoline->getName().mangled());
					if(!constr)
					{
						_error_and_exit("required global constructor %s was not found in the module!",
							pair.second->globalConstructorTrampoline->getName().str().c_str());
					}
					else
					{
						constructors.push_back(constr);
					}
				}
			}


			llvm::FunctionType* ft = llvm::FunctionType::get(llvm::Type::getVoidTy(llvm::getGlobalContext()), false);
			llvm::Function* gconstr = llvm::Function::Create(ft, llvm::GlobalValue::ExternalLinkage,
				"__global_constructor_top_level__", this->linkedModule);

			llvm::IRBuilder<> builder(llvm::getGlobalContext());

			llvm::BasicBlock* iblock = llvm::BasicBlock::Create(llvm::getGlobalContext(), "initialiser", gconstr);
			builder.SetInsertPoint(iblock);

			for(auto f : constructors)
			{
				iceAssert(f);
				builder.CreateCall(f);
			}

			builder.CreateRetVoid();


			if(!Compiler::getNoAutoGlobalConstructor())
			{
				// insert a call at the beginning of main().
				llvm::Function* mainfunc = this->linkedModule->getFunction("main");
				if((Compiler::getOutputMode() == ProgOutputMode::Program || Compiler::getOutputMode() == ProgOutputMode::RunJit) && !mainfunc)
					_error_and_exit("No main() function");

				iceAssert(mainfunc);

				llvm::BasicBlock* entry = &mainfunc->getEntryBlock();
				llvm::BasicBlock* f = llvm::BasicBlock::Create(llvm::getGlobalContext(), "__main_entry", mainfunc);

				f->moveBefore(entry);
				builder.SetInsertPoint(f);
				builder.CreateCall(gconstr);
				builder.CreateBr(entry);
			}
		}
	}
}

























































































