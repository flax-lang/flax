// jit.cpp
// Copyright (c) 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include "errors.h"
#include "backends/llvm.h"


#ifdef _MSC_VER
	#pragma warning(push, 0)
	#pragma warning(disable: 4267)
	#pragma warning(disable: 4244)
#else
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif

#include "llvm/Analysis/Passes.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/Scalar/GVN.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"


#ifdef _MSC_VER
	#pragma warning(pop)
#else
	#pragma GCC diagnostic pop
#endif




namespace backend
{
	LLVMJit::LLVMJit() :
		TM(llvm::EngineBuilder().selectTarget()), DL(TM->createDataLayout()),
		ObjectLayer(llvm::AcknowledgeORCv1Deprecation, ES, [this](llvm::orc::VModuleKey K) -> auto {
			return llvm::orc::LegacyRTDyldObjectLinkingLayer::Resources {
				std::make_shared<llvm::SectionMemoryManager>(), Resolvers[K]
			};
		}),
		CompileLayer(llvm::AcknowledgeORCv1Deprecation, ObjectLayer, llvm::orc::SimpleCompiler(*TM)),
		OptimiseLayer(llvm::AcknowledgeORCv1Deprecation, CompileLayer, [this](std::unique_ptr<llvm::Module> M) -> auto {
			return optimiseModule(std::move(M));
		}),
		CompileCallbackManager(cantFail(llvm::orc::createLocalCompileCallbackManager(TM->getTargetTriple(), ES, 0))),
		CODLayer(llvm::AcknowledgeORCv1Deprecation, ES, OptimiseLayer,
			[&](ModuleHandle_t K) { return Resolvers[K]; },
			[&](ModuleHandle_t K, std::shared_ptr<llvm::orc::SymbolResolver> R) {
				Resolvers[K] = std::move(R);
			},
			[](llvm::Function& F) { return std::set<llvm::Function* >({ &F }); },
			*CompileCallbackManager, llvm::orc::createLocalIndirectStubsManagerBuilder(TM->getTargetTriple()))
	{
		llvm::sys::DynamicLibrary::LoadLibraryPermanently(nullptr);
	}

	LLVMJit::ModuleHandle_t LLVMJit::addModule(std::unique_ptr<llvm::Module> mod)
	{
		auto vmod = this->ES.allocateVModule();

		this->Resolvers[vmod] = createLegacyLookupResolver(this->ES, [this](const std::string& name) -> llvm::JITSymbol {
			if(auto Sym = CompileLayer.findSymbol(name, false))
				return Sym;

			else if(auto Err = Sym.takeError())
				return std::move(Err);

			if(auto SymAddr = llvm::RTDyldMemoryManager::getSymbolAddressInProcess(name))
				return llvm::JITSymbol(SymAddr, llvm::JITSymbolFlags::Exported);

			return nullptr;
		}, [](llvm::Error Err) { llvm::cantFail(std::move(Err), "lookupFlags failed"); });

		// Add the module to the JIT with the new key.
		llvm::cantFail(this->CODLayer.addModule(vmod, std::move(mod)));

		return vmod;
	}

	void LLVMJit::removeModule(LLVMJit::ModuleHandle_t mod)
	{
		llvm::cantFail(this->CODLayer.removeModule(mod));
	}

	llvm::JITSymbol LLVMJit::findSymbol(const std::string& name)
	{
		std::string mangledName;
		llvm::raw_string_ostream out(mangledName);
		llvm::Mangler::getNameWithPrefix(out, name, this->DL);

		return this->CODLayer.findSymbol(out.str(), false);
	}

	std::unique_ptr<llvm::Module> LLVMJit::optimiseModule(std::unique_ptr<llvm::Module> mod)
	{
		// Create a function pass manager.
		auto fpm = llvm::make_unique<llvm::legacy::FunctionPassManager>(mod.get());

		// Add some optimisations.
		fpm->add(llvm::createInstructionCombiningPass());
		fpm->add(llvm::createReassociatePass());
		fpm->add(llvm::createGVNPass());
		fpm->add(llvm::createCFGSimplificationPass());
		fpm->doInitialization();

		// Run the optimizations over all functions in the module being added to the JIT.
		for(auto& F : *mod)
			fpm->run(F);

		return mod;
	}

	llvm::JITTargetAddress LLVMJit::getSymbolAddress(const std::string& name)
	{
		auto addr = this->findSymbol(name).getAddress();
		if(!addr)
		{
			std::string err;
			auto out = llvm::raw_string_ostream(err);

			out << addr.takeError();
			error("llvm: failed to find symbol '%s' (%s)", name, out.str());
		}

		return addr.get();
	}
}




















