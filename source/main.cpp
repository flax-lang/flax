// main.cpp
// Copyright (c) 2014 - 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include "defs.h"
#include "errors.h"
#include "backend.h"
#include "frontend.h"

#include "ir/module.h"

#include "mpool.h"
#include "allocator.h"


struct timer
{
	timer(double* t) : out(t)   { start = std::chrono::high_resolution_clock::now(); }
	~timer()                    { if(out) *out = (double) (std::chrono::high_resolution_clock::now() - start).count() / 1000.0 / 1000.0; }
	double stop()               { return (double) (std::chrono::high_resolution_clock::now() - start).count() / 1000.0 / 1000.0; }

	double* out = 0;
	std::chrono::time_point<std::chrono::high_resolution_clock> start;
};


#ifdef _WIN32
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif

extern "C" DLLEXPORT void lmao()
{
	printf("LMAO!\n");
}

extern "C" DLLEXPORT void haha(int x)
{
	for(int i = 0; i < x; i++)
		printf("HA");

	printf("!\n");
}


static void compile(std::string in, std::string out)
{
	auto ts = std::chrono::high_resolution_clock::now();

	double lexer_ms = 0;
	double parser_ms = 0;
	double typecheck_ms = 0;

	timer total(nullptr);

	frontend::CollectorState state;
	sst::DefinitionTree* dtree = 0;
	{
		{
			timer t(&lexer_ms);
			frontend::collectFiles(in, &state);
			// debuglogln("lexed (%.2f) - (a: %.2fk, f: %.2fk, w: %.2fk)", total.stop(),
			// 	mem::getAllocatedCount() / 1024.0, mem::getDeallocatedCount() / 1024.0, mem::getWatermark() / 1024.0);
			// mem::resetStats();
		}

		{
			timer t(&parser_ms);
			frontend::parseFiles(&state);
			// debuglogln("parsed (%.2f) - (a: %.2fk, f: %.2fk, w: %.2fk)", total.stop(),
			// 	mem::getAllocatedCount() / 1024.0, mem::getDeallocatedCount() / 1024.0, mem::getWatermark() / 1024.0);
			// mem::resetStats();
		}

		{
			timer t(&typecheck_ms);

			dtree = frontend::typecheckFiles(&state);
			// debuglogln("typechecked (%.2f) - (a: %.2fk, f: %.2fk, w: %.2fk)", total.stop(),
			// 	mem::getAllocatedCount() / 1024.0, mem::getDeallocatedCount() / 1024.0, mem::getWatermark() / 1024.0);

			// mem::resetStats();

			iceAssert(dtree);
		}
	}

	timer t(nullptr);

	fir::Module* module = frontend::generateFIRModule(&state, dtree);
	auto cd = backend::CompiledData { module };

	// debuglogln("codegened (%.2f) - (a: %.2fk, f: %.2fk, w: %.2fk)", total.stop(),
	// 	mem::getAllocatedCount() / 1024.0, mem::getDeallocatedCount() / 1024.0, mem::getWatermark() / 1024.0);

	auto codegen_ms = t.stop();

	auto compile_ms = (double) (std::chrono::high_resolution_clock::now() - ts).count() / 1000.0 / 1000.0;
	printf("compile took %.1f (lexer: %.1f, parser: %.1f, typecheck: %.1f, codegen: %.1f) ms%s\n",
		compile_ms, lexer_ms, parser_ms, typecheck_ms, codegen_ms,
		compile_ms > 3000 ? strprintf("  (aka %.2f s)", compile_ms / 1000.0).c_str() : "");

	printf("%zu FIR values generated\n", fir::ConstantBool::get(false)->id);

	if(frontend::getPrintFIR())
		fprintf(stderr, "%s\n", module->print().c_str());

	// delete all the memory we've allocated.
	util::clearAllPools();
	// debuglogln("cleared (%.2f) - (a: %.2fk, f: %.2fk, w: %.2fk)", total.stop(),
	// 	mem::getAllocatedCount() / 1024.0, mem::getDeallocatedCount() / 1024.0, mem::getWatermark() / 1024.0);

	{
		using namespace backend;
		Backend* backend = Backend::getBackendFromOption(frontend::getBackendOption(), cd, { in }, out);
		if(backend == 0) return;

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
			error("Selected backend '%s' does not have some required capabilities (missing %s)\n", backend->str(),
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
















