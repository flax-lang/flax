// TypeAliasCodegen.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "ast.h"
#include "codegen.h"

using namespace Ast;
using namespace Codegen;

Result_t TypeAlias::codegen(CodegenInstance* cgi, fir::Value* extra)
{
	return Result_t(0, 0);
}

fir::Type* TypeAlias::getType(CodegenInstance* cgi, bool allowFail, fir::Value* extra)
{
	return 0;
}

fir::Type* TypeAlias::createType(CodegenInstance* cgi)
{
	iceAssert(0);
}






















