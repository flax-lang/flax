// driver.cpp
// Copyright (c) 2019, zhiayang
// Licensed under the Apache License Version 2.0.

#include <chrono>

#include "defs.h"
#include "backend.h"
#include "frontend.h"
#include "platform.h"

#include "ir/interp.h"
#include "ir/module.h"

#include "backends/interp.h"


namespace backend
{
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


	using namespace fir;
	using namespace fir::interp;

	FIRInterpBackend::FIRInterpBackend(CompiledData& dat, const std::vector<std::string>& inputs, const std::string& output)
		: Backend(BackendCaps::JIT, dat, inputs, output)
	{
		platform::compiler::performSelfDlOpen();
	}

	FIRInterpBackend::~FIRInterpBackend()
	{
		if(this->is) delete this->is;
	}

	std::string FIRInterpBackend::str()
	{
		return "FIR Interpreter";
	}

	void FIRInterpBackend::performCompilation()
	{
		this->is = new InterpState(this->compiledData.module);
		this->is->initialise();

		// it suffices to compile just the entry function.
		this->is->compileFunction(this->compiledData.module->getEntryFunction());
	}

	void FIRInterpBackend::optimiseProgram()
	{
		// nothing.
	}

	void FIRInterpBackend::writeOutput()
	{
		if(auto entryfn = this->compiledData.module->getEntryFunction(); entryfn)
		{
			auto ts = std::chrono::high_resolution_clock::now();


			auto f = this->is->compiledFunctions[entryfn];

			// add arguments if necessary, i guess.
			// just make null values.
			std::vector<interp::Value> args;
			for(auto a : entryfn->getArguments())
				args.push_back(this->is->makeValue(a));

			this->is->runFunction(f, args);

			_printTiming(ts, "interp");
		}
		else
		{
			error("interp: no entry function, cannot run!");
		}
	}
}























