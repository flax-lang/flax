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

	std::deque<std::string> scopes;
	scopes.push_back(vrl->name);

	ScopeResolution* nested = nullptr;

	while((nested = dynamic_cast<ScopeResolution*>(this->member)))
	{
		VarRef* vrs = dynamic_cast<VarRef*>(nested->scope);
		if(!vrs)
			GenError::expected(this, "identifier");

		scopes.push_back(vrs->name);
		this->member = nested;
	}


	{
		FuncCall* fc = nullptr;
		if((fc = dynamic_cast<FuncCall*>(this->member)))
		{
			// the funccall generator will try the pure unmangled type first
			// so we just screw with fc->name.

			fc->name = cgi->mangleWithNamespace(fc->name, scopes);
			fc->name = cgi->mangleName(fc->name, fc->params);
		}
	}

	return this->member->codegen(cgi);
}





Result_t NamespaceDecl::codegen(CodegenInstance* cgi)
{
	for(std::string s : this->name)
		cgi->pushNamespaceScope(s);

	auto ret = this->innards->codegen(cgi);

	for(std::string s : this->name)
		cgi->popNamespaceScope();

	return ret;
}









