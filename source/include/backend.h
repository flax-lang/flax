// backend.h
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once

#include "defs.h"
#include "compiler.h"

#ifndef __STDC_CONSTANT_MACROS
#define __STDC_CONSTANT_MACROS
#endif

#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS
#endif

#include "llvm/ADT/SmallVector.h"

namespace llvm
{
	class Module;
	class TargetMachine;
	class ExecutionEngine;
}

namespace Compiler
{
	enum class OptimisationLevel
	{
		Invalid,

		Debug,		// -Ox
		None,		// -O0

		Minimal,	// -O1
		Normal,		// -O2
		Aggressive	// -O3
	};

	enum class ProgOutputMode
	{
		Invalid,

		RunJit,			// -run or -jit
		ObjectFile,		// -c
		LLVMBitcode,	// -emit-llvm
		Program			// (default)
	};

	enum class BackendOption
	{
		Invalid,

		LLVM,
		Assembly_x64,
	};

	namespace BackendCaps
	{
		enum Capabilities : int
		{
			EmitProgram		= 0x01,
			EmitObject		= 0x02,
			EmitAssembly	= 0x04,
			JIT				= 0x08,
		};
	}

	std::string capabilitiesToString(BackendCaps::Capabilities caps);

	struct Backend
	{
		BackendCaps::Capabilities getCapabilities() { return (BackendCaps::Capabilities) this->capabilities; }
		bool hasCapability(BackendCaps::Capabilities cap) { return this->capabilities & cap; }

		static Backend* getBackendFromOption(BackendOption opt, CompiledData& cd, std::deque<std::string> in, std::string out);

		virtual ~Backend() { }

		// in order of calling.
		virtual void performCompilation() = 0;
		virtual void optimiseProgram() = 0;
		virtual void writeOutput() = 0;

		virtual std::string str() = 0;

		protected:
		Backend(int caps, CompiledData& dat, std::deque<std::string> inputs, std::string output)
			: capabilities(caps), compiledData(dat), inputFilenames(inputs), outputFilename(output) { }

		int capabilities;
		CompiledData& compiledData;
		std::deque<std::string> inputFilenames;
		std::string outputFilename;
	};

	struct LLVMBackend : Backend
	{
		LLVMBackend(CompiledData& dat, std::deque<std::string> inputs, std::string output);
		virtual ~LLVMBackend() { }

		virtual void performCompilation() override;
		virtual void optimiseProgram() override;
		virtual void writeOutput() override;

		virtual std::string str() override;

		private:
		void setupTargetMachine();
		void finaliseGlobalConstructors();
		void runProgramWithJIT();
		llvm::SmallVector<char, 0> initialiseLLVMStuff();

		llvm::Module* linkedModule = 0;
		llvm::TargetMachine* targetMachine = 0;
	};

	struct x64Backend : Backend
	{
		x64Backend(CompiledData& dat, std::deque<std::string> inputs, std::string output);
		virtual ~x64Backend() { }

		virtual void performCompilation() override;
		virtual void optimiseProgram() override;
		virtual void writeOutput() override;

		virtual std::string str() override;
	};
}












