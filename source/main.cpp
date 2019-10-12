// main.cpp
// Copyright (c) 2014 - 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include "defs.h"
#include "errors.h"
#include "backend.h"
#include "frontend.h"

#include "ir/module.h"

#include "memorypool.h"
#include "allocator.h"

#include "ir/interp.h"



struct timer
{
	timer() : out(nullptr)              { start = std::chrono::high_resolution_clock::now(); }
	explicit timer(double* t) : out(t)  { start = std::chrono::high_resolution_clock::now(); }
	~timer()        { if(out) *out = static_cast<double>((std::chrono::high_resolution_clock::now() - start).count()) / 1000000.0; }
	double stop()   { return static_cast<double>((std::chrono::high_resolution_clock::now() - start).count()) / 1000000.0; }

	double* out = 0;
	std::chrono::time_point<std::chrono::high_resolution_clock> start;
};



static void compile(std::string in, std::string out)
{
	auto start_time = std::chrono::high_resolution_clock::now();

	double lexer_ms = 0;
	double parser_ms = 0;
	double typecheck_ms = 0;

	timer total;

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
			}
		}

		{
			timer t(&parser_ms);
			frontend::parseFiles(&state);

			if(frontend::getPrintProfileStats())
			{
				debuglogln("parse   (%.1f ms)\t[w: %.1fk, f: %.1fk, a: %.1fk]", total.stop(), mem::getWatermark() / 1024.0,
					mem::getDeallocatedCount() / 1024.0, mem::getAllocatedCount() / 1024.0);
			}
		}

		{
			timer t(&typecheck_ms);
			dtree = frontend::typecheckFiles(&state);

			if(frontend::getPrintProfileStats())
			{
				debuglogln("typechk (%.1f ms)\t[w: %.1fk, f: %.1fk, a: %.1fk]", total.stop(), mem::getWatermark() / 1024.0,
					mem::getDeallocatedCount() / 1024.0, mem::getAllocatedCount() / 1024.0);
			}

			iceAssert(dtree);
		}
	}

	timer t;


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
		auto compile_ms = static_cast<double>((std::chrono::high_resolution_clock::now() - start_time).count()) / 1000.0 / 1000.0;

		debuglogln("cleared (%.1f ms)\t[w: %.1fk, f: %.1fk, a: %.1fk]", total.stop(), mem::getWatermark() / 1024.0,
			mem::getDeallocatedCount() / 1024.0, mem::getAllocatedCount() / 1024.0);

		debuglogln("compile (%.1f ms)\t[lex: %.1f, parse: %.1f, typechk: %.1f, codegen: %.1f]",
			compile_ms, lexer_ms, parser_ms, typecheck_ms, codegen_ms);

		debuglogln("%d lines, %.2f loc/s, %d fir values\n", state.totalLinesOfCode,
			static_cast<double>(state.totalLinesOfCode) / (compile_ms / 1000.0),
			fir::Value::getCurrentValueId());
	}


	if(frontend::getPrintFIR())
		fprintf(stderr, "%s\n", module->print().c_str());

	{
		#if !OS_DARWIN
			if(frontend::getFrameworksToLink().size() > 0 || frontend::getFrameworkSearchPaths().size() > 0)
				error("backend: frameworks are only supported on Darwin");
		#endif


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

		if(backend->hasCapability(static_cast<BackendCaps::Capabilities>(capsneeded)))
		{
			backend->performCompilation();
			backend->optimiseProgram();
			backend->writeOutput();
		}
		else
		{
			error("selected backend '%s' does not have some required capabilities (missing %s)\n", backend->str(),
				capabilitiesToString(static_cast<BackendCaps::Capabilities>(capsneeded)));
		}
	}
}



int main(int argc, char** argv)
{
	platform::setupTerminalIfNecessary();
	platform::compiler::performSelfDlOpen();

	auto [ input_file, output_file ] = frontend::parseCmdLineOpts(argc, argv);
	compile(input_file, output_file);
	return 0;
}
















