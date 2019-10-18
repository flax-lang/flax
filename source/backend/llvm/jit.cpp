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


static std::string dealWithLLVMError(const llvm::Error& err)
{
	std::string str;
	auto out = llvm::raw_string_ostream(str);

	out << err;
	return out.str();
}

namespace backend
{
	LLVMJit::LLVMJit(llvm::orc::JITTargetMachineBuilder JTMB, llvm::DataLayout DL) : ObjectLayer(ES, []() {
			return llvm::make_unique<llvm::SectionMemoryManager>();
		}),
        CompileLayer(ES, ObjectLayer, llvm::orc::ConcurrentIRCompiler(std::move(JTMB))),
        OptimiseLayer(ES, CompileLayer, optimiseModule),
        DL(std::move(DL)), Mangle(ES, this->DL),
        Ctx(llvm::make_unique<llvm::LLVMContext>())
	{
		llvm::sys::DynamicLibrary::LoadLibraryPermanently(nullptr);
		ES.getMainJITDylib().setGenerator(llvm::cantFail(
			llvm::orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(DL.getGlobalPrefix())));

		// dunno who's bright idea it was to match symbol flags *EXACTLY* instead of something more sane
		ObjectLayer.setOverrideObjectFlagsWithResponsibilityFlags(true);
		ObjectLayer.setAutoClaimResponsibilityForObjectSymbols(true);
	}

	void LLVMJit::addModule(std::unique_ptr<llvm::Module> mod)
	{
		// store it first lest it get stolen away
		auto modIdent = mod->getModuleIdentifier();

		// llvm::Error::operator bool() returns true if there's an error.
		if(auto err = OptimiseLayer.add(ES.getMainJITDylib(), llvm::orc::ThreadSafeModule(std::move(mod), Ctx)); err)
			error("llvm: failed to add module '%s': %s", modIdent, dealWithLLVMError(err));
	}

	llvm::JITEvaluatedSymbol LLVMJit::findSymbol(const std::string& name)
	{
		if(auto ret = ES.lookup({ &ES.getMainJITDylib() }, Mangle(name)); !ret)
			error("llvm: failed to find symbol '%s': %s", name, dealWithLLVMError(ret.takeError()));

		else
			return ret.get();
	}



	LLVMJit* LLVMJit::create()
	{
		auto JTMB = llvm::orc::JITTargetMachineBuilder::detectHost();
		if(!JTMB) error("llvm: failed to detect host", dealWithLLVMError(JTMB.takeError()));

		auto DL = JTMB->getDefaultDataLayoutForTarget();
		if(!DL) error("llvm: failed to get data layout", dealWithLLVMError(DL.takeError()));

		return new LLVMJit(std::move(*JTMB), std::move(*DL));
	}


	llvm::Expected<llvm::orc::ThreadSafeModule> LLVMJit::optimiseModule(llvm::orc::ThreadSafeModule TSM,
		const llvm::orc::MaterializationResponsibility& R)
	{
		 // Create a function pass manager.
		auto FPM = llvm::make_unique<llvm::legacy::FunctionPassManager>(TSM.getModule());

		// Add some optimizations.
		FPM->add(llvm::createInstructionCombiningPass());
		FPM->add(llvm::createReassociatePass());
		FPM->add(llvm::createGVNPass());
		FPM->add(llvm::createCFGSimplificationPass());
		FPM->doInitialization();

		// Run the optimizations over all functions in the module being added to
		// the JIT.
		for(auto& F : *TSM.getModule())
			FPM->run(F);

		return TSM;
	}

	llvm::JITTargetAddress LLVMJit::getSymbolAddress(const std::string& name)
	{
		return this->findSymbol(name).getAddress();
	}
}




















