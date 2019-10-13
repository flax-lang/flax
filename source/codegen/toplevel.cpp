// toplevel.cpp
// Copyright (c) 2014 - 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include "sst.h"
#include "errors.h"
#include "codegen.h"
#include "typecheck.h"

#include "ir/type.h"
#include "ir/module.h"
#include "ir/interp.h"
#include "ir/irbuilder.h"

#include "memorypool.h"

namespace cgn
{
	static size_t csid = 0;
	CodegenState::CodegenState(const fir::IRBuilder& i) : irb(i)
	{
		this->id = csid++;
	}

	fir::Module* codegen(sst::DefinitionTree* dtr)
	{
		auto mod = new fir::Module(dtr->base->name);
		auto builder = fir::IRBuilder(mod);

		auto cs = new CodegenState(builder);
		cs->module = mod;

		cs->typeDefnMap = dtr->typeDefnMap;
		cs->compilerSupportDefinitions = dtr->compilerSupportDefinitions;

		{
			cs->pushLoc(dtr->topLevel);
			defer(cs->popLoc());

			dtr->topLevel->codegen(cs);
			cs->finishGlobalInitFunction();
		}

		mod->setEntryFunction(cs->entryFunction.first);

		delete cs;
		return mod;
	}
}




CGResult sst::NamespaceDefn::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	for(auto stmt : this->statements)
		stmt->codegen(cs);

	return CGResult(0);
}











