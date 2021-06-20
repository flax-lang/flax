// main.cpp
// Copyright (c) 2014 - 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include "defs.h"
#include "errors.h"
#include "backend.h"
#include "frontend.h"

#include "ir/module.h"
#include "ir/interp.h"

#include "memorypool.h"
#include "allocator.h"

#include <chrono>


struct timer
{
	using hrc = std::chrono::high_resolution_clock;

	timer() : out(nullptr)              { start = hrc::now(); }
	explicit timer(double* t) : out(t)  { start = hrc::now(); }
	~timer()                            { if(out) *out = static_cast<double>((hrc::now() - start).count()) / 1000000.0; }
	double measure()                    { return static_cast<double>((hrc::now() - start).count()) / 1000000.0; }

	double* out = 0;
	std::chrono::time_point<hrc> start;
};


static void compile(std::string in, std::string out)
{
	auto start_time = std::chrono::high_resolution_clock::now();

	double lexer_ms     = 0;
	double parser_ms    = 0;
	double typecheck_ms = 0;
	double codegen_ms   = 0;

	timer total;

	auto printStats = [&total](const std::string& name) {
		if(frontend::getPrintProfileStats())
		{
			debuglogln("%-9s (%.1f ms)\t[w: %.1fk, f: %.1fk, a: %.1fk]", name, total.measure(), mem::getWatermark() / 1024.0,
				mem::getDeallocatedCount() / 1024.0, mem::getAllocatedCount() / 1024.0);
		}
	};


	auto cd = backend::CompiledData();
	{
		frontend::CollectorState state;
		sst::DefinitionTree* dtree = 0;

		{
			timer t(&lexer_ms);
			frontend::collectFiles(in, &state);
			printStats("lex");
		}

		{
			timer t(&parser_ms);
			frontend::parseFiles(&state);
			printStats("parse");
		}

		{
			timer t(&typecheck_ms);
			dtree = frontend::typecheckFiles(&state);
			printStats("typecheck");
		}

		{
			timer t(&codegen_ms);
			iceAssert(dtree);

			auto module = frontend::generateFIRModule(&state, dtree);
			module->finaliseGlobalConstructors();
			printStats("codegen");

			// if we requested to dump, then dump the IR here.
			if(frontend::getPrintFIR())
				fprintf(stderr, "%s\n", module->print().c_str());


			cd.module = module;
		}


		// delete *most* of the memory we've allocated. obviously IR values need to stay alive,
		// since we haven't run the backend yet. so this just kills the AST and SST values.
		util::clearAllPools();
		printStats("free_mem");


		// print the final stats.
		if(frontend::getPrintProfileStats())
		{
			auto compile_ms = static_cast<double>((std::chrono::high_resolution_clock::now() - start_time).count()) / 1000.0 / 1000.0;


			debuglogln("%-9s (%.1f ms)\t[lex: %.1f, parse: %.1f, typechk: %.1f, codegen: %.1f]", "compile",
				compile_ms, lexer_ms, parser_ms, typecheck_ms, codegen_ms);

			debuglogln("processed: %d lines, %.2f loc/s, %d fir values\n", state.totalLinesOfCode,
				static_cast<double>(state.totalLinesOfCode) / (compile_ms / 1000.0),
				fir::Value::getCurrentValueId());
		}
	}

	iceAssert(cd.module);




	{
		#if !OS_DARWIN
			if(frontend::getFrameworksToLink().size() > 0 || frontend::getFrameworkSearchPaths().size() > 0)
				error("backend: frameworks are only supported on Darwin");
		#endif


		using namespace backend;
		Backend* backend = Backend::getBackendFromOption(frontend::getBackendOption(), cd, { in }, out);
		if(backend == 0) return;

		int _capsneeded = 0;
		{
			if(frontend::getOutputMode() == ProgOutputMode::RunJit)
				_capsneeded |= BackendCaps::JIT;

			if(frontend::getOutputMode() == ProgOutputMode::ObjectFile)
				_capsneeded |= BackendCaps::EmitObject;

			if(frontend::getOutputMode() == ProgOutputMode::Program)
				_capsneeded |= BackendCaps::EmitProgram;
		}
		auto capsneeded = static_cast<BackendCaps::Capabilities>(_capsneeded);


		if(backend->hasCapability(capsneeded))
		{
			backend->performCompilation();
			backend->optimiseProgram();
			backend->writeOutput();
		}
		else
		{
			error("selected backend '%s' does not have some required capabilities (missing %s)\n", backend->str(),
				capabilitiesToString(capsneeded));
		}
	}
}



int main(int argc, char** argv)
{
	platform::setupCrashHandlers();
	platform::setupTerminalIfNecessary();
	platform::compiler::performSelfDlOpen();

	auto [ input_file, output_file ] = frontend::parseCmdLineOpts(argc, argv);
	compile(input_file, output_file);


	return 0;
}
















