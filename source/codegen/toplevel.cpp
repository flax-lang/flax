// toplevel.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "sst.h"
#include "errors.h"
#include "codegen.h"
#include "typecheck.h"

#include "ir/type.h"
#include "ir/module.h"
#include "ir/irbuilder.h"

namespace cgn
{
	fir::Module* codegen(sst::DefinitionTree* dtr)
	{
		// debuglog("codegen for %s\n", dtr->base->name.c_str());

		auto mod = new fir::Module(dtr->base->name);
		auto builder = fir::IRBuilder(fir::getDefaultFTContext());

		auto cs = new CodegenState(builder);
		cs->stree = dtr->base;
		cs->module = mod;

		cs->pushLoc(dtr->topLevel);
		defer(cs->popLoc());

		for(auto stmt : dtr->topLevel->statements)
			stmt->codegen(cs);

		// debuglog("\n\n\n%s\n\n", cs->module->print().c_str());
		mod->setEntryFunction(cs->entryFunction.first);

		return cs->module;
	}
}




CGResult sst::NamespaceDefn::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	cs->enterNamespace(this->name);

	for(auto stmt : this->statements)
		stmt->codegen(cs);

	cs->leaveNamespace();
	return CGResult(0);
}











