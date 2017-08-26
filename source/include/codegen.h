// codegen.h
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once
#include "defs.h"
#include "ir/irbuilder.h"

namespace fir
{
	struct Module;
	struct IRBuilder;
}

namespace sst
{
	struct Stmt;
	struct StateTree;
	struct DefinitionTree;
}

namespace cgn
{
	struct CodegenState
	{
		CodegenState(const fir::IRBuilder& i) : irb(i) { }
		fir::Module* module = 0;
		sst::StateTree* stree = 0;

		fir::IRBuilder irb;

		std::pair<fir::Function*, Location> entryFunction = { };

		std::vector<Location> locationStack;

		void pushLoc(const Location& loc);
		void pushLoc(sst::Stmt* stmt);
		void popLoc();

		Location loc();

		void enterNamespace(std::string name);
		void leaveNamespace();
	};

	fir::Module* codegen(sst::DefinitionTree* dtr);
}







