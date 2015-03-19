// NamespaceCodegen.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "../include/ast.h"
#include "../include/codegen.h"
#include "../include/llvm_all.h"

using namespace Ast;
using namespace Codegen;

// recursively resolves the scope, and returns only the right-most thing (probably a function call or type ref or whatever)
static void recursiveResolveScope(ScopeResolution* _sr, CodegenInstance* cgi, std::deque<std::string>* scopes)
{
	// lhs and rhs are probably var refs
	ScopeResolution* sr	= nullptr;
	VarRef* left = nullptr;

	if((left = dynamic_cast<VarRef*>(_sr->scope)))
		scopes->push_back(left->name);

	else if((sr = dynamic_cast<ScopeResolution*>(_sr->scope)))
		recursiveResolveScope(sr, cgi, scopes);

	else
		error(_sr, "Unexpected expr type %s", typeid(*_sr->scope).name());


	VarRef* vr = nullptr;
	if((vr = dynamic_cast<VarRef*>(_sr->member)))
		scopes->push_back(vr->name);
}



static Expr* resolveScope(ScopeResolution* _sr, CodegenInstance* cgi, std::deque<std::string>* scopes)
{
	// lhs and rhs are probably var refs
	ScopeResolution* sr	= nullptr;
	VarRef* left = nullptr;

	if((left = dynamic_cast<VarRef*>(_sr->scope)))
		scopes->push_back(left->name);

	else if((sr = dynamic_cast<ScopeResolution*>(_sr->scope)))
		recursiveResolveScope(sr, cgi, scopes);

	else
		error(_sr, "Unexpected expr type %s", typeid(*_sr->scope).name());

	return _sr->member;
}

Expr* ScopeResolution::getActualExpr(CodegenInstance* cgi)
{
	std::deque<std::string> scopes;
	Expr* result = resolveScope(this, cgi, &scopes);

	return result;
}

Result_t ScopeResolution::codegen(CodegenInstance* cgi, llvm::Value* lhsPtr, llvm::Value* rhs)
{
	std::deque<std::string> scopes;
	Expr* result = resolveScope(this, cgi, &scopes);

	// printf("sr codegen: %s\n", typeid(*result).name());
	// for(auto s : scopes) printf("::%s", s.c_str());
	// printf("\n");

	FuncCall* fc = dynamic_cast<FuncCall*>(result);
	if(fc)
	{
		// printf("%lld, fc scope res (%s)\n", this->posinfo.line, fc->name.c_str());
		// the funccall generator will try the pure unmangled type first
		// so we just screw with fc->name.

		fc->name = cgi->mangleWithNamespace(fc->name, scopes);
		fc->name = cgi->mangleName(fc->name, fc->params);

	}
	else
	{
		printf("???: %s\n", typeid(*result).name());
	}


	return result->codegen(cgi);
}






















