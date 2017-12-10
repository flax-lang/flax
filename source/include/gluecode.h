// gluecode.h
// Copyright (c) 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include <stdint.h>

#include <string>
#include <vector>

#define DEBUG_RUNTIME_GLUE_MASTER	0

#define DEBUG_STRING_MASTER			(0 & DEBUG_RUNTIME_GLUE_MASTER)
#define DEBUG_STRING_ALLOCATION		(0 & DEBUG_STRING_MASTER)
#define DEBUG_STRING_REFCOUNTING	(0 & DEBUG_STRING_MASTER)

#define DEBUG_ARRAY_MASTER			(1 & DEBUG_RUNTIME_GLUE_MASTER)
#define DEBUG_ARRAY_ALLOCATION		(1 & DEBUG_ARRAY_MASTER)
#define DEBUG_ARRAY_REFCOUNTING		(0 & DEBUG_ARRAY_MASTER)

namespace cgn { struct CodegenState; }
namespace fir { struct Value; }

namespace cgn {
namespace glue
{
	void printError(cgn::CodegenState* cs, fir::Value* pos, std::string msg, std::vector<fir::Value*> args);
}
}