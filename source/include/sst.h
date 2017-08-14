// sst.h
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "defs.h"
#include "parser.h"

namespace sst
{
	struct SemanticState
	{

	};

	SemanticState* typecheck(const parser::ParsedFile& file);
}
