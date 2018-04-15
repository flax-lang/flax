// main.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "defs.h"
#include "errors.h"
#include "backend.h"
#include "frontend.h"

#include "ir/module.h"

static void compile(std::string in, std::string out)
{
	auto ts = std::chrono::high_resolution_clock::now();

	auto module = frontend::collectFiles(in);
	auto cd = backend::CompiledData { module };

	auto dur = std::chrono::high_resolution_clock::now() - ts;
	auto ms = (double) dur.count() / 1000.0 / 1000.0;
	auto s = ms / 1000.0;
	fprintf(stderr, "compilation (excluding llvm) took %.1f ms  (aka %.2f s)\n", ms, s);
	return;

	{
		using namespace backend;
		Backend* backend = Backend::getBackendFromOption(frontend::getBackendOption(), cd, { in }, out);

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
			error("Selected backend '%s' does not have some required capabilities (missing '%s')\n", backend->str(),
				capabilitiesToString((BackendCaps::Capabilities) capsneeded));
		}
	}
}



int main(int argc, char** argv)
{
	auto [ input_file, output_file ] = frontend::parseCmdLineOpts(argc, argv);
	compile(input_file, output_file);
	return 0;
}
















