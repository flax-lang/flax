// parse.cpp
// Copyright (c) 2019, zhiayang
// Licensed under the Apache License Version 2.0.

#include "repl.h"
#include "parser.h"
#include "frontend.h"
#include "parser_internal.h"

#include "codegen.h"
#include "typecheck.h"

#include "ir/module.h"
#include "ir/irbuilder.h"

#include "memorypool.h"

// defined in codegen/directives.cpp
fir::ConstantValue* magicallyRunExpressionAtCompileTime(cgn::CodegenState* cs, sst::Stmt* stmt, fir::Type* infer, const Identifier& fname);

namespace repl
{
	struct State
	{
		State()
		{
			auto modname = "__repl_mod__";

			this->module = new fir::Module(modname);

			sst::StateTree* tree = new sst::StateTree(modname, modname, 0);
			tree->treeDefn = util::pool<sst::TreeDefn>(Location());
			tree->treeDefn->tree = tree;

			this->fs = new sst::TypecheckState(tree);
			this->cs = new cgn::CodegenState(fir::IRBuilder(this->module));
			this->cs->module = this->module;

			// so we don't crash, give us a starting location.
			this->cs->pushLoc(Location());
		}

		fir::Module* module;
		sst::TypecheckState* fs;
		cgn::CodegenState* cs;

		size_t counter = 0;
	};

	static State* state = 0;
	void setupEnvironment()
	{
		state = new State();
	}

	bool processLine(const std::string& line)
	{
		std::string replName = "<repl>";

		frontend::CollectorState collector;

		// lex.
		platform::cachePreExistingFile(replName, line);
		auto lexResult = frontend::lexTokensFromString(replName, line);

		// parse, but first setup the environment.
		auto st = parser::State(lexResult.tokens);
		auto _stmt = parser::parseStmt(st, /* exprs: */ true);

		if(_stmt.needsMoreTokens())
		{
			return true;
		}
		else if(_stmt.isError())
		{
			_stmt.err()->post();
			return false;
		}

		// there's no need to fiddle with AST-level trees -- once we typecheck it,
		// it will store the relevant state into the TypecheckState.
		{
			auto stmt = _stmt.val();
			auto tcr = stmt->typecheck(state->fs);

			if(tcr.isError())
			{
				tcr.error()->post();
				return false;
			}
			else if(!tcr.isParametric() && !tcr.isDummy())
			{
				// copy some stuff over.
				state->cs->typeDefnMap = state->fs->typeDefnMap;

				// ok, we have a thing. try to run it.

				auto value = magicallyRunExpressionAtCompileTime(state->cs, tcr.stmt(), nullptr,
					Identifier(zpr::sprint("__anon_runner_%d", state->counter++), IdKind::Name));

				if(value)
					printf("%s\n", zpr::sprint("%s  ::  %s", value->str(), value->getType()).c_str());
			}
		}






		return false;
	}
}















