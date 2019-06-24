// driver.cpp
// Copyright (c) 2019, zhiayang
// Licensed under the Apache License Version 2.0.

#include "defs.h"
#include "backend.h"
#include "platform.h"

#include "ir/interp.h"
#include "ir/module.h"

#include "backends/interp.h"


namespace backend
{
	using namespace fir;
	using namespace fir::interp;

	FIRInterpBackend::FIRInterpBackend(CompiledData& dat, std::vector<std::string> inputs, std::string output)
		: Backend(BackendCaps::JIT, dat, inputs, output)
	{
		platform::performSelfDlOpen();
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
			auto f = this->is->compiledFunctions[entryfn];

			// add arguments if necessary, i guess.
			// just make null values.
			std::vector<interp::Value> args;
			for(auto a : entryfn->getArguments())
				args.push_back(this->is->makeValue(a));

			this->is->runFunction(f, args);
		}
		else
		{
			error("interp: no entry function, cannot run!");
		}
	}
}























