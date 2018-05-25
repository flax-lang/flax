// jit.cpp
// Copyright (c) 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "backends/llvm.h"

namespace backend
{
	LLVMJit::LLVMJit(llvm::TargetMachine* tm) :
		objectLayer([]() -> auto { return std::make_shared<llvm::SectionMemoryManager>(); }),
		compileLayer(this->objectLayer, llvm::orc::SimpleCompiler(*tm))
	{
		this->targetMachine = std::unique_ptr<llvm::TargetMachine>(tm);

		llvm::sys::DynamicLibrary::LoadLibraryPermanently(nullptr);
	}

	llvm::TargetMachine* LLVMJit::getTargetMachine()
	{
		return this->targetMachine.get();
	}

	LLVMJit::ModuleHandle_t LLVMJit::addModule(std::unique_ptr<llvm::Module> mod)
	{
		auto resolver = llvm::orc::createLambdaResolver([&](const std::string& name) -> auto {
			if(auto sym = this->compileLayer.findSymbol(name, false))
				return sym;

			else
				return llvm::JITSymbol(nullptr);
		}, [](const std::string& name) -> auto {
			if(auto symaddr = llvm::RTDyldMemoryManager::getSymbolAddressInProcess(name))
				return llvm::JITSymbol(symaddr, llvm::JITSymbolFlags::Exported);

			else
				return llvm::JITSymbol(nullptr);
		});

		return llvm::cantFail(this->compileLayer.addModule(std::move(mod), std::move(resolver)));
	}

	void LLVMJit::removeModule(LLVMJit::ModuleHandle_t mod)
	{
		llvm::cantFail(this->compileLayer.removeModule(mod));
	}

	llvm::JITSymbol LLVMJit::findSymbol(const std::string& name)
	{
		std::string mangledName;
		llvm::raw_string_ostream out(mangledName);
		llvm::Mangler::getNameWithPrefix(out, name, this->targetMachine->createDataLayout());

		return this->compileLayer.findSymbol(out.str(), true);
	}

	llvm::JITTargetAddress LLVMJit::getSymbolAddress(const std::string& name)
	{
		return llvm::cantFail(this->findSymbol(name).getAddress());
	}

}




















