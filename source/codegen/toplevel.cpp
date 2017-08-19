// toplevel.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "sst.h"
#include "errors.h"
#include "codegen.h"
#include "typecheck.h"

#include "ir/type.h"
#include "ir/module.h"

namespace cgn
{
	fir::Module* codegen(sst::DefinitionTree* dtr)
	{
		warn("codegen for %s\n", dtr->base->name.c_str());
		return 0;
	}
}



