// codegen.h
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once
#include "defs.h"

namespace fir
{
	struct Module;
}

namespace sst
{
	struct DefinitionTree;
}

namespace cgn
{
	struct CodegenState
	{
	};

	fir::Module* codegen(sst::DefinitionTree* dtr);
}
