// FuncCallCodegen.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "../include/ast.h"
#include "../include/codegen.h"
#include "../include/llvm_all.h"

using namespace Ast;
using namespace Codegen;

Result_t FuncCall::codegen(CodegenInstance* cgi)
{
	FuncPair_t* fp = cgi->getDeclaredFunc(this->name);
	std::string cmangled = "";
	std::string cppmangled = "";

	if(!fp)	fp = cgi->getDeclaredFunc(cmangled = cgi->mangleName(this->name, this->params));
	if(!fp)	fp = cgi->getDeclaredFunc(cppmangled = cgi->mangleCppName(this->name, this->params));

	if(!fp)
		GenError::unknownSymbol(this, this->name + ", tried (c): " + cmangled + ", (c++): " + cppmangled, SymbolType::Function);

	llvm::Function* target = fp->first;
	if((target->arg_size() != this->params.size() && !target->isVarArg()) || (target->isVarArg() && target->arg_size() > 0 && this->params.size() == 0))
		error(this, "Expected %ld arguments, but got %ld arguments instead", target->arg_size(), this->params.size());

	std::vector<llvm::Value*> args;



	// we need to get the function declaration
	FuncDecl* decl = fp->second;
	if(decl)
	{
		for(int i = 0; i < this->params.size(); i++)
			this->params[i] = cgi->autoCastType(decl->params[i], this->params[i]);
	}

	for(Expr* e : this->params)
		args.push_back(e->codegen(cgi).result.first);

	return Result_t(cgi->mainBuilder.CreateCall(target, args), 0);
}
