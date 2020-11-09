// linker.cpp
// Copyright (c) 2014 - 2016, zhiayang
// Licensed under the Apache License Version 2.0.



#ifndef __STDC_CONSTANT_MACROS
#define __STDC_CONSTANT_MACROS
#endif

#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS
#endif

#include <fstream>


#ifdef _MSC_VER
	#pragma warning(push, 0)
	#pragma warning(disable: 4267)
	#pragma warning(disable: 4244)
#else
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif

#include "llvm/IR/Verifier.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Support/Host.h"
#include "llvm/Linker/Linker.h"
#include "llvm/Transforms/IPO.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/Analysis/Passes.h"
#include "llvm/Transforms/Utils.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Bitcode/BitcodeWriter.h"
#include "llvm/ExecutionEngine/MCJIT.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/ToolOutputFile.h"
#include "llvm/Support/DynamicLibrary.h"
#include "llvm/Transforms/Scalar/Scalarizer.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"

#ifdef _MSC_VER
	#pragma warning(pop)
#else
	#pragma GCC diagnostic pop
#endif

#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "ir/type.h"
#include "ir/value.h"
#include "ir/module.h"
#include "ir/irbuilder.h"

#include "frontend.h"
#include "backends/llvm.h"

#include "tinyprocesslib/tinyprocess.h"


static llvm::LLVMContext globalContext;

template <typename T>
static void _printTiming(T ts, const std::string& thing)
{
	if(frontend::getPrintProfileStats())
	{
		auto dur = std::chrono::high_resolution_clock::now() - ts;
		auto ms = static_cast<double>(dur.count()) / 1000000.0;
		printf("%s took %.1f ms%s\n", thing.c_str(), ms, ms > 3000 ? strprintf("  (aka %.2f s)", ms / 1000.0).c_str() : "");
	}
}

namespace backend
{
	llvm::LLVMContext& LLVMBackend::getLLVMContext()
	{
		return globalContext;
	}

	LLVMBackend::LLVMBackend(CompiledData& dat, const std::vector<std::string>& inputs, const std::string& output)
		: Backend(BackendCaps::EmitAssembly | BackendCaps::EmitObject | BackendCaps::EmitProgram | BackendCaps::JIT, dat, inputs, output)
	{
	}

	std::string LLVMBackend::str()
	{
		return "LLVM";
	}

	void LLVMBackend::performCompilation()
	{
		auto ts = std::chrono::high_resolution_clock::now();

		llvm::InitializeNativeTarget();

		auto mainModule = this->translateFIRtoLLVM(this->compiledData.module);

		if(this->compiledData.module->getEntryFunction())
			this->entryFunction = mainModule->getFunction(this->compiledData.module->getEntryFunction()->getName().mangled());

		this->linkedModule = std::unique_ptr<llvm::Module>(mainModule);

		// ok, move some shit into here because llvm is fucking retarded
		this->setupTargetMachine();
		this->linkedModule->setDataLayout(this->targetMachine->createDataLayout());

		_printTiming(ts, "llvm translation");
	}


	void LLVMBackend::optimiseProgram()
	{
		auto ts = std::chrono::high_resolution_clock::now();

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

		fpm.run(*this->linkedModule);

		_printTiming(ts, "llvm opt");

		if(frontend::getPrintLLVMIR())
			this->linkedModule->print(llvm::outs(), 0);
	}

	void LLVMBackend::writeOutput()
	{
		auto ts = std::chrono::high_resolution_clock::now();

		if(llvm::verifyModule(*this->linkedModule, &llvm::errs()))
		{
			fprintf(stderr, "\n\n");
			this->linkedModule->print(llvm::errs(), 0);

			BareError::make("llvm: module verification failed")->postAndQuit();
		}

		std::string oname;
		if(this->outputFilename.empty())
		{
			auto base = this->linkedModule->getModuleIdentifier();

			if(frontend::getOutputMode() == ProgOutputMode::ObjectFile)
				oname = platform::compiler::getObjectFileName(base);

			else
				oname = platform::compiler::getExecutableName(base);
		}
		else
		{
			oname = this->outputFilename;
		}


		if(frontend::getOutputMode() == ProgOutputMode::RunJit)
		{
			std::string modname = ("llvm-jit-" + this->linkedModule->getModuleIdentifier());
			const char* argv = modname.c_str();

			auto entry = this->getEntryFunctionFromJIT();

			_printTiming(ts, "llvm jit");
			printf("\n");

			iceAssert(this->jitInstance);
			entry(1, &argv);

			delete this->jitInstance;
		}
		else if(frontend::getOutputMode() == ProgOutputMode::LLVMBitcode)
		{
			std::error_code e;
			llvm::sys::fs::OpenFlags of = static_cast<llvm::sys::fs::OpenFlags>(0);
			llvm::raw_fd_ostream rso(oname.c_str(), e, of);

			llvm::WriteBitcodeToFile(*this->linkedModule.get(), rso);
			rso.close();

			_printTiming(ts, "writing bitcode file");
		}
		else
		{
			if(frontend::getOutputMode() != ProgOutputMode::ObjectFile && !this->compiledData.module->getEntryFunction())
			{
				error("llvm: no entry function marked, a program cannot be compiled");
			}

			llvm::SmallVector<char, 0> buffer;
			{
				auto bufferStream = std::make_unique<llvm::raw_svector_ostream>(buffer);
				llvm::raw_pwrite_stream* rawStream = bufferStream.get();

				{
					llvm::legacy::PassManager pm = llvm::legacy::PassManager();
					targetMachine->addPassesToEmitFile(pm, *rawStream, nullptr,
						llvm::CodeGenFileType::CGFT_ObjectFile);

					pm.run(*this->linkedModule);
				}

				// flush and kill it.
				rawStream->flush();
			}


			if(frontend::getOutputMode() == ProgOutputMode::ObjectFile)
			{
				// now memoryBuffer should contain the .object file
				std::ofstream objectOutput(oname, std::ios::binary | std::ios::out);
				objectOutput.write(buffer.data(), buffer.size_in_bytes());
				objectOutput.close();
			}
			else
			{
				std::string objname = platform::compiler::getObjectFileName(this->linkedModule->getModuleIdentifier());

				std::ofstream objectOutput(objname, std::ios::binary | std::ios::out);
				objectOutput.write(buffer.data(), buffer.size_in_bytes());
				objectOutput.close();

				auto cmdline = platform::compiler::getCompilerCommandLine({ objname }, oname);

				// debuglogln("link cmdline:\n%s", cmdline);

				std::string sout;
				std::string serr;

				tinyproclib::Process proc(cmdline, "", [&sout](const char* bytes, size_t n) {
					sout = std::string(bytes, n);
				}, [&serr](const char* bytes, size_t n) {
					serr = std::string(bytes, n);
				});

				// note: this waits for the process to finish.
				int status = proc.get_exit_status();

				if(status != 0)
				{
					if(!sout.empty()) fprintf(stderr, "%s\n", sout.c_str());
					if(!serr.empty()) fprintf(stderr, "%s\n", serr.c_str());

					fprintf(stderr, "linker returned non-zero (status = %d), exiting\n", status);
					fprintf(stderr, "cmdline was: %s\n", cmdline.c_str());
					exit(status);
				}
			}

			_printTiming(ts, "outputting exe/obj");
		}
	}










	void LLVMBackend::setupTargetMachine()
	{
		llvm::InitializeNativeTarget();
		llvm::InitializeNativeTargetAsmParser();
		llvm::InitializeNativeTargetAsmPrinter();

		llvm::Triple targetTriple;
		if(frontend::getOutputMode() == ProgOutputMode::RunJit || frontend::getParameter("targetarch").empty())
		{
			targetTriple.setTriple(llvm::sys::getProcessTriple());
		}
		else
		{
			targetTriple.setTriple(frontend::getParameter("targetarch"));
		}

		std::string err_str;
		const llvm::Target* theTarget = llvm::TargetRegistry::lookupTarget("", targetTriple, err_str);
		if(!theTarget)
		{
			error("llvm: failed in creating target: (wanted: '%s'); llvm error: %s\n", targetTriple.str(), err_str);
		}


		// get the mcmodel
		llvm::CodeModel::Model codeModel;

		auto getMcModelOfString = [](const std::string& s) -> llvm::CodeModel::Model {
			if(s == "kernel")   return llvm::CodeModel::Kernel;
			if(s == "small")    return llvm::CodeModel::Small;
			if(s == "medium")   return llvm::CodeModel::Medium;
			if(s == "large")    return llvm::CodeModel::Large;
			else                error("llvm: invalid mcmodel '%s' (valid options: kernel, small, medium, or large)", s);
		};

		auto getDefaultCodeModelForTarget = [](const llvm::Triple& triple) -> llvm::CodeModel::Model {
			if(triple.isArch64Bit() && !triple.isOSDarwin() && !triple.isAndroid() && !(triple.isAArch64() && triple.isOSLinux()))
			{
				return llvm::CodeModel::Large;
			}
			else if(triple.isOSDarwin() && (frontend::getOutputMode() == ProgOutputMode::RunJit))
			{
				// apparently, when we JIT on osx we need mcmodel=large
				// but when we are compiling, we need mcmodel=small!!
				// WTF? nobody on the internet seems to know, but this was determined experimentally.
				// if we use mcmodel=small when JIT-ing, we crash when running...
				return llvm::CodeModel::Large;
			}
			else
			{
				return llvm::CodeModel::Small;
			}
		};

		if(auto mcm = frontend::getParameter("mcmodel"); !mcm.empty())
			codeModel = getMcModelOfString(mcm);

		else
			codeModel = getDefaultCodeModelForTarget(targetTriple);


		llvm::TargetOptions targetOptions;
		llvm::Reloc::Model relocModel = llvm::Reloc::Model::Static;

		// todo: use dynamic no pic for dylibs?? idk
		if(frontend::getIsPositionIndependent())
			relocModel = llvm::Reloc::Model::PIC_;

		this->targetMachine = theTarget->createTargetMachine(targetTriple.getTriple(), "", "",
			targetOptions, relocModel, codeModel, llvm::CodeGenOpt::Default);
	}






















	EntryPoint_t LLVMBackend::getEntryFunctionFromJIT()
	{
		using namespace llvm::sys;

		platform::compiler::addLibrarySearchPaths();

		// default libraries come with the correct prefix/extension for the platform already, but user ones do not.
		auto tolink = zfu::map(frontend::getLibrariesToLink(), [](auto lib) -> auto {
			return platform::compiler::getSharedLibraryName(lib);
		}) + platform::compiler::getDefaultSharedLibraries();

		for(auto lib : tolink)
		{
			std::string err;
			auto dl = DynamicLibrary::getPermanentLibrary(lib.c_str(), &err);
			if(!dl.isValid())
				error("llvm: failed to load library '%s', dlopen failed with error:\n%s", lib, err);
		}

		for(auto fw : frontend::getFrameworksToLink())
		{
			auto name = strprintf("%s.framework/%s", fw, fw);

			std::string err;
			auto dl = DynamicLibrary::getPermanentLibrary(name.c_str(), &err);
			if(!dl.isValid())
				error("llvm: failed to load framework '%s', dlopen failed with error:\n%s", fw, err);
		}


		EntryPoint_t ret = 0;
		iceAssert(this->entryFunction);
		{
			auto name = this->entryFunction->getName().str();

			this->jitInstance = LLVMJit::create();
			this->jitInstance->addModule(std::move(this->linkedModule));

			auto entryaddr = this->jitInstance->getSymbolAddress(name);
			ret = reinterpret_cast<EntryPoint_t>(entryaddr);

			iceAssert(ret && "failed to resolve entry function address");
		}

		platform::compiler::restoreLibrarySearchPaths();
		return ret;
	}
}


















































































