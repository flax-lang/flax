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

#include "llvm/PassManager.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Support/Host.h"
#include "llvm/Linker/Linker.h"
#include "llvm/Analysis/Passes.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/ExecutionEngine/MCJIT.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Instrumentation.h"
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
		Ast::Root* root = std::get<0>(data);
		std::vector<std::string> filelist = std::get<1>(data);
		std::unordered_map<std::string, Ast::Root*> rootmap = std::get<2>(data);
		std::unordered_map<std::string, llvm::Module*> modulelist;

		// translate to llvm
		for(auto mod : std::get<3>(data))
		{
			modulelist[mod.first] = mod.second->translateToLlvm();
			optimiseLlvmModule(modulelist[mod.first]);

			if(Compiler::getDumpFir())
				printf("%s\n\n\n\n", mod.second->print().c_str());
		}



		// link together
		llvm::Module* mainModule = modulelist[filename];
		llvm::IRBuilder<> builder(llvm::getGlobalContext());

		llvm::Linker linker = llvm::Linker(mainModule);
		for(auto mod : modulelist)
		{
			if(mod.second != mainModule)
				linker.linkInModule(mod.second);
		}

		mainModule = linker.getModule();

		doGlobalConstructors(filename, data, root, mainModule);


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


		for(auto s : std::get<1>(data))
			remove(s.c_str());


		// cleanup
		for(auto p : rootmap)
			delete p.second;
	}



































	static void runProgramWithJit(llvm::Module* mod)
	{
		// all linked already.
		// dump here, before the output.

		llvm::verifyModule(*mod, &llvm::errs());
		if(mod->getFunction("main") != 0)
		{
			std::string err;
			llvm::ExecutionEngine* ee = llvm::EngineBuilder(std::unique_ptr<llvm::Module>(mod))
						.setErrorStr(&err)
						.setMCJITMemoryManager(llvm::make_unique<llvm::SectionMemoryManager>())
						.create();

			void* func = ee->getPointerToFunction(mod->getFunction("main"));
			iceAssert(func);
			auto mainfunc = (int (*)(int, const char**)) func;

			const char* m[] = { ("__llvmJIT_" + mod->getModuleIdentifier()).c_str() };

			// finalise the object, which causes the memory to be executable
			// fucking NX bit
			ee->finalizeObject();
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
		Compiler::compileProgram(mod, std::get<1>(data), foldername, outname);
	}

	static void doGlobalConstructors(std::string filename, CompiledData& data, Ast::Root* root, llvm::Module* mod)
	{
		auto& rootmap = std::get<2>(data);

		bool needGlobalConstructor = false;
		if(root->globalConstructorTrampoline != 0) needGlobalConstructor = true;
		for(auto pair : std::get<2>(data))
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
		llvm::FunctionPassManager functionPassManager = llvm::FunctionPassManager(mod);

		if(Compiler::getOptimisationLevel() > 0)
		{
			// Provide basic AliasAnalysis support for GVN.
			functionPassManager.add(llvm::createBasicAliasAnalysisPass());

			// Do simple "peephole" optimisations and bit-twiddling optzns.
			functionPassManager.add(llvm::createInstructionCombiningPass());

			// Reassociate expressions.
			functionPassManager.add(llvm::createReassociatePass());

			// Eliminate Common SubExpressions.
			functionPassManager.add(llvm::createGVNPass());


			// Simplify the control flow graph (deleting unreachable blocks, etc).
			functionPassManager.add(llvm::createCFGSimplificationPass());

			// hmm.
			// fuck it, turn everything on.
			functionPassManager.add(llvm::createLoadCombinePass());
			functionPassManager.add(llvm::createConstantHoistingPass());
			functionPassManager.add(llvm::createLICMPass());
			functionPassManager.add(llvm::createDelinearizationPass());
			functionPassManager.add(llvm::createFlattenCFGPass());
			functionPassManager.add(llvm::createScalarizerPass());
			functionPassManager.add(llvm::createSinkingPass());
			functionPassManager.add(llvm::createStructurizeCFGPass());
			functionPassManager.add(llvm::createInstructionSimplifierPass());
			functionPassManager.add(llvm::createDeadStoreEliminationPass());
			functionPassManager.add(llvm::createDeadInstEliminationPass());
			functionPassManager.add(llvm::createMemCpyOptPass());

			functionPassManager.add(llvm::createSCCPPass());
			functionPassManager.add(llvm::createAggressiveDCEPass());

			functionPassManager.add(llvm::createTailCallEliminationPass());
		}

		// optimisation level -1 disables *everything*
		// mostly for reading the IR to debug codegen.
		if(Compiler::getOptimisationLevel() >= 0)
		{
			// always do the mem2reg pass, our generated code is too inefficient
			functionPassManager.add(llvm::createPromoteMemoryToRegisterPass());
			functionPassManager.add(llvm::createMergedLoadStoreMotionPass());
			functionPassManager.add(llvm::createScalarReplAggregatesPass());
			functionPassManager.add(llvm::createConstantPropagationPass());
			functionPassManager.add(llvm::createDeadCodeEliminationPass());
		}

		functionPassManager.doInitialization();

		for(auto& f : mod->getFunctionList())
			functionPassManager.run(f);
	}
}






















































