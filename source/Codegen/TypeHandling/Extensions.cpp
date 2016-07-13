// ExtensionCodegen.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "ast.h"
#include "codegen.h"

using namespace Ast;
using namespace Codegen;

Result_t ExtensionDef::codegen(CodegenInstance* cgi, fir::Value* extra)
{
	warn("lol");
	return Result_t(0, 0);
}


fir::Type* ExtensionDef::createType(CodegenInstance* cgi, std::map<std::string, fir::Type*> instantiatedGenericTypes)
{
	return 0;
}














