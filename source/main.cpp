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

#include "ir/interp.h"



struct timer
{
	timer(double* t) : out(t)   { start = std::chrono::high_resolution_clock::now(); }
	~timer()                    { if(out) *out = (double) (std::chrono::high_resolution_clock::now() - start).count() / 1000.0 / 1000.0; }
	double stop()               { return (double) (std::chrono::high_resolution_clock::now() - start).count() / 1000.0 / 1000.0; }

	double* out = 0;
	std::chrono::time_point<std::chrono::high_resolution_clock> start;
};



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

			if(frontend::getPrintProfileStats())
			{
				debuglogln("lex     (%.1f ms)\t[w: %.1fk, f: %.1fk, a: %.1fk]", total.stop(), mem::getWatermark() / 1024.0,
					mem::getDeallocatedCount() / 1024.0, mem::getAllocatedCount() / 1024.0);
				// mem::resetStats();
			}
		}

		{
			timer t(&parser_ms);
			frontend::parseFiles(&state);

			if(frontend::getPrintProfileStats())
			{
				debuglogln("parse   (%.1f ms)\t[w: %.1fk, f: %.1fk, a: %.1fk]", total.stop(), mem::getWatermark() / 1024.0,
					mem::getDeallocatedCount() / 1024.0, mem::getAllocatedCount() / 1024.0);
				// mem::resetStats();
			}
		}

		{
			timer t(&typecheck_ms);
			dtree = frontend::typecheckFiles(&state);

			if(frontend::getPrintProfileStats())
			{
				debuglogln("typechk (%.1f ms)\t[w: %.1fk, f: %.1fk, a: %.1fk]", total.stop(), mem::getWatermark() / 1024.0,
					mem::getDeallocatedCount() / 1024.0, mem::getAllocatedCount() / 1024.0);
				// mem::resetStats();
			}

			iceAssert(dtree);
		}
	}

	timer t(nullptr);

	platform::performSelfDlOpen();

	fir::Module* module = frontend::generateFIRModule(&state, dtree);
	module->finaliseGlobalConstructors();


	auto cd = backend::CompiledData { module };

	if(frontend::getPrintProfileStats())
	{
		debuglogln("codegen (%.1f ms)\t[w: %.1fk, f: %.1fk, a: %.1fk]", total.stop(), mem::getWatermark() / 1024.0,
			mem::getDeallocatedCount() / 1024.0, mem::getAllocatedCount() / 1024.0);
	}

	auto codegen_ms = t.stop();

	// delete all the memory we've allocated.
	util::clearAllPools();

	if(frontend::getPrintProfileStats())
	{
		auto compile_ms = (double) (std::chrono::high_resolution_clock::now() - ts).count() / 1000.0 / 1000.0;

		debuglogln("cleared (%.1f ms)\t[w: %.1fk, f: %.1fk, a: %.1fk]", total.stop(), mem::getWatermark() / 1024.0,
			mem::getDeallocatedCount() / 1024.0, mem::getAllocatedCount() / 1024.0);
		debuglogln("compile (%.1f ms)\t[l: %.1f, p: %.1f, t: %.1f, c: %.1f]", compile_ms, lexer_ms, parser_ms, typecheck_ms, codegen_ms);
		debuglogln("%zu FIR values generated\n", fir::Value::getCurrentValueId());
	}


	if(frontend::getPrintFIR())
		fprintf(stderr, "%s\n", module->print().c_str());

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
			backend->performCompilation();
			backend->optimiseProgram();
			backend->writeOutput();
		}
		else
		{
			error("selected backend '%s' does not have some required capabilities (missing %s)\n", backend->str(),
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
















