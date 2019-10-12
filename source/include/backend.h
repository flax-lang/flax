// backend.h
// Copyright (c) 2014 - 2016, zhiayang
// Licensed under the Apache License Version 2.0.

#pragma once

#include "defs.h"

namespace fir
{
	struct Module;
	struct Function;
}

namespace backend
{
	struct CompiledData
	{
		fir::Module* module = 0;
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

		None,
		LLVM,
		Interpreter,
		Assembly_x64,
	};

	std::string capabilitiesToString(BackendCaps::Capabilities caps);

	struct Backend
	{
		BackendCaps::Capabilities getCapabilities() { return static_cast<BackendCaps::Capabilities>(this->capabilities); }
		bool hasCapability(BackendCaps::Capabilities cap) { return this->capabilities & cap; }

		static Backend* getBackendFromOption(BackendOption opt, CompiledData& cd, std::vector<std::string> in, std::string out);

		virtual ~Backend() { }

		// in order of calling.
		virtual void performCompilation() = 0;
		virtual void optimiseProgram() = 0;
		virtual void writeOutput() = 0;

		virtual std::string str() = 0;

		protected:
		Backend(int caps, CompiledData& dat, std::vector<std::string> inputs, std::string output)
			: capabilities(caps), compiledData(dat), inputFilenames(inputs), outputFilename(output) { }

		int capabilities;
		CompiledData& compiledData;
		std::vector<std::string> inputFilenames;
		std::string outputFilename;
	};

	struct x64Backend : Backend
	{
		x64Backend(CompiledData& dat, std::vector<std::string> inputs, std::string output);
		virtual ~x64Backend() { }

		virtual void performCompilation() override;
		virtual void optimiseProgram() override;
		virtual void writeOutput() override;

		virtual std::string str() override;
	};
}












