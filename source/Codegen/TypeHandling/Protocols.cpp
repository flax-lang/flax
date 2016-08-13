// Protocols.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "ast.h"
#include "codegen.h"

using namespace Ast;
using namespace Codegen;

fir::Type* ProtocolDef::createType(Codegen::CodegenInstance* cgi, std::unordered_map<std::string, fir::Type*> instantiatedGenericTypes)
{


	return 0;
}

Result_t ProtocolDef::codegen(Codegen::CodegenInstance* cgi, fir::Value* extra)
{
	return Result_t(0, 0);
}
