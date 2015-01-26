// LoopCodegen.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "../include/ast.h"
#include "../include/codegen.h"
#include "../include/llvm_all.h"

using namespace Ast;
using namespace Codegen;

ValPtr_p WhileLoop::codegen(CodegenInstance* cgi)
{
	return ValPtr_p(0, 0);
}
