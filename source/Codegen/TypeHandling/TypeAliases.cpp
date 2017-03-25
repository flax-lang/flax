// TypeAliasCodegen.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "ast.h"
#include "codegen.h"

using namespace Ast;
using namespace Codegen;

Result_t TypeAlias::codegen(CodegenInstance* cgi, fir::Type* extratype, fir::Value* target)
{
	return Result_t(0, 0);
}

fir::Type* TypeAlias::getType(CodegenInstance* cgi, fir::Type* extratype, bool allowFail)
{
	return 0;
}




















