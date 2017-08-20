// main.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "defs.h"
#include "errors.h"
#include "backend.h"
#include "frontend.h"

int main(int argc, char** argv)
{
	auto [ input_file, output_file ] = frontend::parseCmdLineOpts(argc, argv);

	auto module = frontend::collectFiles(input_file);
	auto cd = backend::CompiledData { .module = module };

	{
		using namespace backend;
		Backend* backend = Backend::getBackendFromOption(frontend::getBackendOption(), cd, { input_file }, output_file);

		int capsneeded = 0;
		{
			if(frontend::getOutputMode() == ProgOutputMode::RunJit)
				capsneeded |= BackendCaps::JIT;

			if(frontend::getOutputMode() == ProgOutputMode::ObjectFile)
				capsneeded |= BackendCaps::EmitObject;

			if(frontend::getOutputMode() == ProgOutputMode::Program)
				capsneeded |= BackendCaps::EmitProgram;
		}

		if(backend->hasCapability((BackendCaps::Capabilities) capsneeded))
		{
			// auto p = prof::Profile(PROFGROUP_LLVM, "llvm_total");
			backend->performCompilation();
			backend->optimiseProgram();
			backend->writeOutput();
		}
		else
		{
			fprintf(stderr, "Selected backend '%s' does not have some required capabilities (missing '%s')\n", backend->str().c_str(),
				capabilitiesToString((BackendCaps::Capabilities) capsneeded).c_str());

			exit(-1);
		}
	}

	return 0;
}
