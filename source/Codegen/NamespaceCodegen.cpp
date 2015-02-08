// NamespaceCodegen.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "../include/ast.h"
#include "../include/codegen.h"
#include "../include/llvm_all.h"

using namespace Ast;
using namespace Codegen;

Result_t ScopeResolution::codegen(CodegenInstance* cgi)
{
	// lhs and rhs are probably var refs
	VarRef* vrl = nullptr;

	if(!(vrl = dynamic_cast<VarRef*>(this->scope)))
		GenError::expected(this, "identifier");

	std::string sscope = vrl->name;


	return Result_t(0, 0);
}





Result_t NamespaceDecl::codegen(CodegenInstance* cgi)
{
	return Result_t(0, 0);
}









