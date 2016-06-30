// LlvmCompiler.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.



#ifndef __STDC_CONSTANT_MACROS
#define __STDC_CONSTANT_MACROS
#endif

#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS
#endif

#include <deque>
#include <vector>

#include <unistd.h>

#include "llvm/IR/Verifier.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Support/Host.h"
#include "llvm/Linker/Linker.h"
#include "llvm/Transforms/IPO.h"
#include "llvm/Analysis/Passes.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/ExecutionEngine/MCJIT.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/ToolOutputFile.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Instrumentation.h"
#include "llvm/CodeGen/MIRParser/MIRParser.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/SectionMemoryManager.h"


#include "ir/type.h"
#include "ir/value.h"
#include "ir/module.h"
#include "ir/irbuilder.h"

#include "compiler.h"


namespace Compiler
{
	// optimise
	static void optimiseLlvmModule(llvm::Module* mod);
	static void doGlobalConstructors(std::string filename, CompiledData& data, Ast::Root* root, llvm::Module* mod);

	static void runProgramWithJit(llvm::Module* mod);
	static void compileBinary(std::string filename, std::string outname, CompiledData data, llvm::Module* mod);


	void compileToLlvm(std::string filename, std::string outname, CompiledData data)
	{
		std::unordered_map<std::string, llvm::Module*> modulelist;

		// translate to llvm
		for(auto mod : data.moduleList)
		{
			modulelist[mod.first] = mod.second->translateToLlvm();

			if(Compiler::getDumpFir())
				printf("%s\n\n\n\n", mod.second->print().c_str());

			// modulelist[mod.first]->dump();
		}


		// link together
		// llvm::Module* mainModule = modulelist[filename];
		llvm::IRBuilder<> builder(llvm::getGlobalContext());

		llvm::Module* mainModule = new llvm::Module("_empty", llvm::getGlobalContext());
		llvm::Linker linker = llvm::Linker(*mainModule);

		for(auto mod : modulelist)
		{
			linker.linkInModule(std::unique_ptr<llvm::Module>(mod.second));
		}


		doGlobalConstructors(filename, data, data.rootNode, mainModule);

		if(Compiler::getDumpLlvm())
			mainModule->dump();

		// once more
		optimiseLlvmModule(mainModule);

		if(Compiler::getRunProgramWithJit())
		{
			runProgramWithJit(mainModule);
		}
		else
		{
			compileBinary(filename, outname, data, mainModule);
		}


		for(auto s : data.fileList)
			std::remove(s.c_str());


		// cleanup
		for(auto p : data.rootMap)
			delete p.second;
	}



































	static void runProgramWithJit(llvm::Module* mod)
	{
		// all linked already.
		// dump here, before the output.

		llvm::verifyModule(*mod, &llvm::errs());
		if(mod->getFunction("main") != 0)
		{
			// std::string err;
			// llvm::ExecutionEngine* ee = llvm::EngineBuilder(std::unique_ptr<llvm::Module>(mod))
			// 			.setErrorStr(&err)
			// 			.setMCJITMemoryManager(llvm::make_unique<llvm::SectionMemoryManager>())
			// 			.create();

			// void* func = ee->getPointerToFunction(mod->getFunction("main"));

			llvm::ExecutionEngine* execEngine = llvm::EngineBuilder(std::unique_ptr<llvm::Module>(mod)).create();
			uint64_t func = execEngine->getFunctionAddress("main");
			iceAssert(func != 0);

			auto mainfunc = (int (*)(int, const char**)) func;

			const char* m[] = { ("__llvmJIT_" + mod->getModuleIdentifier()).c_str() };

			// finalise the object, which causes the memory to be executable
			// fucking NX bit
			// ee->finalizeObject();

			mainfunc(1, m);
		}
		else
		{
			error("no main() function, cannot JIT");
		}
	}

	static void compileBinary(std::string filename, std::string outname, CompiledData data, llvm::Module* mod)
	{
		std::string foldername;
		size_t sep = filename.find_last_of("\\/");
		if(sep != std::string::npos)
			foldername = filename.substr(0, sep);

		llvm::verifyModule(*mod, &llvm::errs());
		Compiler::compileProgram(mod, data.fileList, foldername, outname);
	}

	static void doGlobalConstructors(std::string filename, CompiledData& data, Ast::Root* root, llvm::Module* mod)
	{
		auto& rootmap = data.rootMap;

		bool needGlobalConstructor = false;
		if(root->globalConstructorTrampoline != 0) needGlobalConstructor = true;
		for(auto pair : data.rootMap)
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
			rootmap[filename] = root;

			for(auto pair : rootmap)
			{
				if(pair.second->globalConstructorTrampoline != 0)
				{
					llvm::Function* constr = mod->getFunction(pair.second->globalConstructorTrampoline->getName());
					if(!constr)
					{
						error("required global constructor %s was not found in the module!",
							pair.second->globalConstructorTrampoline->getName().c_str());
					}
					else
					{
						constructors.push_back(constr);
					}
				}
			}

			rootmap.erase(filename);

			llvm::FunctionType* ft = llvm::FunctionType::get(llvm::Type::getVoidTy(llvm::getGlobalContext()), false);
			llvm::Function* gconstr = llvm::Function::Create(ft, llvm::GlobalValue::ExternalLinkage,
				"__global_constructor_top_level__", mod);

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
				llvm::Function* mainfunc = mod->getFunction("main");
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



	static void optimiseLlvmModule(llvm::Module* mod)
	{
		llvm::legacy::PassManager fpm = llvm::legacy::PassManager();

		if(Compiler::getOptimisationLevel() > 0)
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
			fpm.add(llvm::createAggressiveDCEPass());


			// module-level stuff
			fpm.add(llvm::createMergeFunctionsPass());
			fpm.add(llvm::createLoopSimplifyPass());
		}

		// optimisation level -1 disables *everything*
		// mostly for reading the IR to debug codegen.
		if(Compiler::getOptimisationLevel() >= 0)
		{
			// always do the mem2reg pass, our generated code is too inefficient
			fpm.add(llvm::createPromoteMemoryToRegisterPass());
			fpm.add(llvm::createMergedLoadStoreMotionPass());
			fpm.add(llvm::createScalarReplAggregatesPass());
			fpm.add(llvm::createConstantPropagationPass());
			fpm.add(llvm::createDeadCodeEliminationPass());
			fpm.add(llvm::createLoadCombinePass());
		}

		fpm.run(*mod);
	}







	void writeBitcode(std::string oname, llvm::Module* module)
	{
		std::error_code e;
		llvm::sys::fs::OpenFlags of = (llvm::sys::fs::OpenFlags) 0;
		llvm::raw_fd_ostream rso(oname.c_str(), e, of);

		llvm::WriteBitcodeToFile(module, rso);
		rso.close();
	}


	void compileProgram(llvm::Module* module, std::vector<std::string> filelist, std::string foldername, std::string outname)
	{
		std::string tgt;
		if(!getTarget().empty())
			tgt = "-target " + getTarget();


		if(!Compiler::getIsCompileOnly() && !module->getFunction("main"))
		{
			error(0, "No main() function, a program cannot be compiled.");
		}



		std::string oname = outname.empty() ? (foldername + "/" + module->getModuleIdentifier()).c_str() : outname.c_str();

		llvm::verifyModule(*module, &llvm::errs());


		if(Compiler::getEmitLLVMOutput())
		{
			Compiler::writeBitcode(oname + ".bc", module);
			auto it = std::find(filelist.begin(), filelist.end(), oname + ".bc");
			iceAssert(it != filelist.end());

			filelist.erase(it);
		}
		else
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
				error("error creating target: (wanted: '%s');\n"
						"llvm error: %s", targetTriple.str().c_str(), err_str.c_str());
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
				error("Invalid mcmodel '%s' (valid options: kernel, small, medium, or large)", Compiler::getCodeModel().c_str());
			}


			llvm::TargetOptions targetOptions;
			if(Compiler::getIsPositionIndependent())
				targetOptions.PositionIndependentExecutable = true;

			std::unique_ptr<llvm::TargetMachine> targetMachine(theTarget->createTargetMachine(targetTriple.getTriple(),
				"", "", targetOptions, llvm::Reloc::Default, codeModel, llvm::CodeGenOpt::Default));



			std::error_code e;
			auto outStream = llvm::make_unique<llvm::tool_output_file>(oname + ".o", e, llvm::sys::fs::OpenFlags::F_None);

			module->setDataLayout(targetMachine->createDataLayout());

			{
				llvm::raw_pwrite_stream& rawStream = outStream->os();
				llvm::legacy::PassManager pm = llvm::legacy::PassManager();

				targetMachine->addPassesToEmitFile(pm, rawStream, llvm::TargetMachine::CodeGenFileType::CGFT_ObjectFile);

				pm.run(*module);

				outStream->keep();
			}

			if(!Compiler::getIsCompileOnly())
			{
				const char* argv[5];
				argv[0] = "cc";
				argv[1] = "-o";
				argv[2] = "build/test";
				argv[3] = "build/test.o";
				argv[4] = nullptr;


				pid_t pid = fork();
				if(pid == 0)
				{
					// in child, pid == 0.

					execvp(argv[0], (char* const*) argv);
					exit(1);
				}

				// else
				// {
				// 	int stat = 0;
				// 	waitpid(pid, &stat, 0);
				// }


				// std::remove((oname + ".o").c_str());
			}
		}
	}
}






















































