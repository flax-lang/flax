// LLVMBackend.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.



#ifndef __STDC_CONSTANT_MACROS
#define __STDC_CONSTANT_MACROS
#endif

#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS
#endif


#include <fstream>

#include "llvm/IR/Verifier.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Support/Host.h"
#include "llvm/Linker/Linker.h"
#include "llvm/Transforms/IPO.h"
#include "llvm/IR/LLVMContext.h"
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

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "ir/type.h"
#include "ir/value.h"
#include "ir/module.h"
#include "ir/irbuilder.h"

#include "backend.h"
#include "frontend.h"

#include "../../external/tinyprocesslib/process.h"


static llvm::LLVMContext globalContext;

#ifdef _WIN32
errno_t _putenv_s(const char* name, const char* value);
#endif


namespace backend
{
	llvm::LLVMContext& LLVMBackend::getLLVMContext()
	{
		return globalContext;
	}

	LLVMBackend::LLVMBackend(CompiledData& dat, std::vector<std::string> inputs, std::string output) : Backend(BackendCaps::EmitAssembly | BackendCaps::EmitObject | BackendCaps::EmitProgram | BackendCaps::JIT, dat, inputs, output)
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
		iceAssert(llvm::InitializeNativeTarget() == 0);
		iceAssert(llvm::InitializeNativeTargetAsmParser() == 0);
		iceAssert(llvm::InitializeNativeTargetAsmPrinter() == 0);

		// auto p = prof::Profile(PROFGROUP_LLVM, "llvm_linkmod");

		auto mainModule = this->translateFIRtoLLVM(this->compiledData.module);
		auto s = frontend::getFilenameFromPath(this->inputFilenames[0]);

		if(this->compiledData.module->getEntryFunction())
			this->entryFunction = mainModule->getFunction(this->compiledData.module->getEntryFunction()->getName().str());


		this->linkedModule = mainModule;
		this->finaliseGlobalConstructors();

		// ok, move some shit into here because llvm is fucking retarded
		this->setupTargetMachine();
		this->linkedModule->setDataLayout(this->targetMachine->createDataLayout());
	}


	void LLVMBackend::optimiseProgram()
	{
		// auto p = prof::Profile(PROFGROUP_LLVM, "llvm_optimise");

		if(llvm::verifyModule(*this->linkedModule, &llvm::errs()))
		{
			exitless_error("\nLLVM Module verification failed");
			this->linkedModule->dump();

			doTheExit();
		}

		llvm::legacy::PassManager fpm = llvm::legacy::PassManager();

		fpm.add(llvm::createDeadInstEliminationPass());
		fpm.add(llvm::createDeadCodeEliminationPass());

		if(frontend::getOptLevel() > OptimisationLevel::Debug)
		{
			// mem2reg is based, because it changes inefficient load-store branches into more efficient phi-nodes (at least more efficient
			// in terms of optimisation potential)
			fpm.add(llvm::createPromoteMemoryToRegisterPass());
			fpm.add(llvm::createMergedLoadStoreMotionPass());
			fpm.add(llvm::createInstructionCombiningPass());
			fpm.add(llvm::createConstantPropagationPass());
			fpm.add(llvm::createLoadCombinePass());
			fpm.add(llvm::createScalarizerPass());
		}

		if(frontend::getOptLevel() > OptimisationLevel::None)
		{
			fpm.add(llvm::createReassociatePass());
			fpm.add(llvm::createCFGSimplificationPass());

			// hmm.
			// fuck it, turn everything on.

			fpm.add(llvm::createConstantHoistingPass());
			fpm.add(llvm::createLICMPass());
			fpm.add(llvm::createDelinearizationPass());
			fpm.add(llvm::createFlattenCFGPass());
			fpm.add(llvm::createScalarizerPass());
			fpm.add(llvm::createSinkingPass());
			fpm.add(llvm::createInstructionSimplifierPass());
			fpm.add(llvm::createDeadStoreEliminationPass());
			fpm.add(llvm::createMemCpyOptPass());

			fpm.add(llvm::createSCCPPass());

			fpm.add(llvm::createTailCallEliminationPass());
		}

		if(frontend::getOptLevel() > OptimisationLevel::Minimal)
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
		// if(Compiler::getDumpLlvm())
		// fprintf(stderr, "%s\n\n\n", this->compiledData.module->print().c_str());
		this->linkedModule->dump();

		// verify the module.
		{
			// auto p = prof::Profile(PROFGROUP_LLVM, "llvm_verify");
			if(llvm::verifyModule(*this->linkedModule, &llvm::errs()))
				_error_and_exit("\n\nLLVM Module verification failed");
		}

		std::string foldername;
		size_t sep = this->inputFilenames[0].find_last_of("\\/");
		if(sep != std::string::npos)
			foldername = this->inputFilenames[0].substr(0, sep);

		bool wasEmpty = this->outputFilename.empty();
		std::string oname = this->outputFilename.empty() ? (foldername + "/" + this->linkedModule->getModuleIdentifier()).c_str()
			: this->outputFilename.c_str();

		if(frontend::getOutputMode() == ProgOutputMode::RunJit)
		{
			this->runProgramWithJIT();
		}
		else if(frontend::getOutputMode() == ProgOutputMode::LLVMBitcode)
		{
			std::error_code e;
			llvm::sys::fs::OpenFlags of = (llvm::sys::fs::OpenFlags) 0;
			llvm::raw_fd_ostream rso(oname.c_str(), e, of);

			llvm::WriteBitcodeToFile(this->linkedModule, rso);
			rso.close();
		}
		else
		{
			// auto p = prof::Profile(PROFGROUP_LLVM, "llvm_compile");

			if(frontend::getOutputMode() != ProgOutputMode::ObjectFile && !this->compiledData.module->getEntryFunction())
			{
				_error_and_exit("No entry function marked, a program cannot be compiled");
			}

			auto buffer = this->initialiseLLVMStuff();

			if(frontend::getOutputMode() == ProgOutputMode::ObjectFile)
			{
				// if(this->linkedModule->getFunction("main") == 0)
				// {
				// 	fprintf(stderr, "No main() function, a program cannot be compiled\n");
				// 	exit(-1);
				// }

				// now memoryBuffer should contain the .object file
				std::ofstream objectOutput(oname + (wasEmpty ? ".o" : ""), std::ios::binary | std::ios::out);
				objectOutput.write(buffer.data(), buffer.size_in_bytes());
				objectOutput.close();
			}
			else
			{
				std::string objname = "/tmp/flax_" + this->linkedModule->getModuleIdentifier();

				int fd = open(objname.c_str(), O_RDWR | O_CREAT, S_IRWXU);
				if(fd == -1)
				{
					exitless_error("Unable to create temporary file (%s) for linking", objname.c_str());
					perror("open(2) error");

					doTheExit();
				}

				write(fd, buffer.data(), buffer.size_in_bytes());
				close(fd);


				auto libs = frontend::getLibrariesToLink();
				auto libdirs = frontend::getLibrarySearchPaths();

				auto frames = frontend::getFrameworksToLink();
				auto framedirs = frontend::getFrameworkSearchPaths();


				// here, if we're doing a link, and we're not in freestanding mode, then we're going to add -lc and -lm
				if(!frontend::getIsFreestanding())
				{
					if(std::find(libs.begin(), libs.end(), "m") == libs.end())
						libs.push_back("m");

					if(std::find(libs.begin(), libs.end(), "c") == libs.end())
						libs.push_back("c");
				}


				size_t num_extra = 0;
				size_t s = 5 + num_extra + (2 * libs.size()) + (2 * libdirs.size()) + (2 * frames.size()) + (2 * framedirs.size());
				const char** argv = new const char*[s];
				memset(argv, 0, s * sizeof(const char*));

				argv[0] = "cc";
				argv[1] = "-o";
				argv[2] = oname.c_str();
				argv[3] = objname.c_str();

				size_t i = 4 + num_extra;





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

				argv[s - 1] = 0;

				std::string output;
				int status = 0;




				#if 1
				std::string cmdline;
				for(size_t i = 0; i < s - 1; i++)
				{
					cmdline += argv[i];

					if(strcmp(argv[i], "-l") != 0 && strcmp(argv[i], "-L") != 0)
						cmdline += " ";
				}

				tinyproclib::Process proc(cmdline, "", [&output](const char* bytes, size_t n) {
					output = std::string(bytes, n);
				});

				// oh well.
				// system(cmdline.c_str());



				#else
				int outpipe[2];
				iceAssert(pipe(outpipe) == 0);

				pid_t pid = fork();
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
				#endif

				// delete the temp file
				// std::remove(templ);

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
		// for(auto p : this->compiledData.rootMap)
		//	delete p.second;
	}










	void LLVMBackend::setupTargetMachine()
	{
		llvm::InitializeAllTargets();
		llvm::InitializeAllTargetMCs();
		llvm::InitializeAllAsmParsers();
		llvm::InitializeAllAsmPrinters();

		llvm::PassRegistry* Registry = llvm::PassRegistry::getPassRegistry();
		llvm::initializeCore(*Registry);
		llvm::initializeCodeGen(*Registry);

		llvm::Triple targetTriple;
		targetTriple.setTriple(frontend::getParameter("targetarch").empty() ? llvm::sys::getProcessTriple()
			: frontend::getParameter("targetarch"));



		std::string err_str;
		const llvm::Target* theTarget = llvm::TargetRegistry::lookupTarget("", targetTriple, err_str);
		if(!theTarget)
		{
			_error_and_exit("failed in creating target: (wanted: '%s');\n"
					"llvm error: %s\n", targetTriple.str().c_str(), err_str.c_str());
		}


		// get the mcmodel
		llvm::CodeModel::Model codeModel;
		if(frontend::getParameter("mcmodel") == "kernel")
		{
			codeModel = llvm::CodeModel::Kernel;
		}
		else if(frontend::getParameter("mcmodel") == "small")
		{
			codeModel = llvm::CodeModel::Small;
		}
		else if(frontend::getParameter("mcmodel") == "medium")
		{
			codeModel = llvm::CodeModel::Medium;
		}
		else if(frontend::getParameter("mcmodel") == "large")
		{
			codeModel = llvm::CodeModel::Large;
		}
		else if(frontend::getParameter("mcmodel").empty())
		{
			codeModel = llvm::CodeModel::Default;
		}
		else
		{
			_error_and_exit("Invalid mcmodel '%s' (valid options: kernel, small, medium, or large)",
				frontend::getParameter("mcmodel").c_str());
		}


		llvm::TargetOptions targetOptions;
		llvm::Reloc::Model relocModel = llvm::Reloc::Model::Static;

		// todo: use dynamic no pic for dylibs?? idk
		if(frontend::getIsPositionIndependent())
			relocModel = llvm::Reloc::Model::PIC_;

		this->targetMachine = theTarget->createTargetMachine(targetTriple.getTriple(), "", "",
			targetOptions, relocModel, codeModel, llvm::CodeGenOpt::Default);
	}





	llvm::SmallVector<char, 0> LLVMBackend::initialiseLLVMStuff()
	{
		llvm::SmallVector<char, 0> memoryBuffer;
		auto bufferStream = llvm::make_unique<llvm::raw_svector_ostream>(memoryBuffer);
		llvm::raw_pwrite_stream* rawStream = bufferStream.get();

		{
			// auto p = prof::Profile(PROFGROUP_LLVM, "llvm_emit_object");
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
			for(auto L : frontend::getLibrarySearchPaths())
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
			for(auto F : frontend::getFrameworkSearchPaths())
				fenv += F + ":";

			if(!fenv.empty() && fenv.back() == ':')
				fenv.pop_back();
		}


		// set the things

		#ifndef _WIN32
		{
			setenv("LD_LIBRARY_PATH", env.c_str(), 1);
			setenv("DYLD_FRAMEWORK_PATH", fenv.c_str(), 1);
		}
		#else
		{
			_putenv_s("LD_LIBRARY_PATH", env.c_str());
			_putenv_s("DYLD_FRAMEWORK_PATH", fenv.c_str());
		}
		#endif




		auto tolink = frontend::getLibrariesToLink();

		// note: linux is stupid. to be safe, explicitly link libc and libm
		// note: will not affect freestanding implementations, since this is JIT mode
		// note2: the stupidity of linux extends further than i thought
		// apparently we cannot dlopen "libc.so", because that's not even a fucking ELF library.
		// note3: wow, this applies to libm as well.
		// fuck you torvalds
		// so basically just do nothing.

		for(auto l : tolink)
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
				_error_and_exit("Failed to load library '%s', dlopen failed with error:\n%s", l.c_str(), err.c_str());
		}


		for(auto l : frontend::getFrameworksToLink())
		{
			std::string name = l + ".framework/" + l;

			std::string err;
			llvm::sys::DynamicLibrary dl = llvm::sys::DynamicLibrary::getPermanentLibrary(name.c_str(), &err);
			if(!dl.isValid())
				_error_and_exit("Failed to load framework '%s', dlopen failed with error:\n%s", l.c_str(), err.c_str());
		}



		if(this->entryFunction)
		{
			llvm::ExecutionEngine* execEngine = llvm::EngineBuilder(std::unique_ptr<llvm::Module>(this->linkedModule)).create();

			// finalise the object, which does something.
			execEngine->finalizeObject();

			// uint64_t func = execEngine->getfunct("main");
			void* func = execEngine->getPointerToFunction(this->entryFunction);
			iceAssert(func != 0);

			auto mainfunc = (int (*)(int, const char**)) func;

			const char* m[] = { ("__llvmJIT_" + this->linkedModule->getModuleIdentifier()).c_str() };

			mainfunc(1, m);
		}
		else
		{
			_error_and_exit("No entry function marked, cannot JIT");
		}


		// restore

		#ifndef _WIN32
		{
			setenv("LD_LIBRARY_PATH", penv.c_str(), 1);
			setenv("DYLD_FRAMEWORK_PATH", pfenv.c_str(), 1);
		}
		#else
		{
			_putenv_s("LD_LIBRARY_PATH", penv.c_str());
			_putenv_s("DYLD_FRAMEWORK_PATH", pfenv.c_str());
		}
		#endif
	}















	void LLVMBackend::finaliseGlobalConstructors()
	{
		// check for a main function somewhere


		#if 0
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


			llvm::FunctionType* ft = llvm::FunctionType::get(llvm::Type::getVoidTy(LLVMBackend::getLLVMContext()), false);
			llvm::Function* gconstr = llvm::Function::Create(ft, llvm::GlobalValue::ExternalLinkage,
				"__global_constructor_top_level__", this->linkedModule);

			llvm::IRBuilder<> builder(LLVMBackend::getLLVMContext());

			llvm::BasicBlock* iblock = llvm::BasicBlock::Create(LLVMBackend::getLLVMContext(), "initialiser", gconstr);
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
				llvm::BasicBlock* f = llvm::BasicBlock::Create(LLVMBackend::getLLVMContext(), "__main_entry", mainfunc);

				f->moveBefore(entry);
				builder.SetInsertPoint(f);
				builder.CreateCall(gconstr);
				builder.CreateBr(entry);
			}
		}
		#endif
	}
}




















































// static std::string _makeCmdLine(const char* fmt, ...)
// {
// 	va_list ap;
// 	va_list ap2;

// 	va_start(ap, fmt);
// 	va_copy(ap2, ap);

// 	ssize_t size = vsnprintf(0, 0, fmt, ap2);

// 	va_end(ap2);


// 	// return -1 to be compliant if
// 	// size is less than 0
// 	iceAssert(size >= 0);

// 	// alloc with size plus 1 for `\0'
// 	char* str = new char[size + 1];

// 	// format string with original
// 	// variadic arguments and set new size
// 	vsprintf(str, fmt, ap);

// 	std::string ret = str;
// 	delete[] str;

// 	return ret;
// };
































