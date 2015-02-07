// FuncCallCodegen.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "../include/ast.h"
#include "../include/codegen.h"
#include "../include/llvm_all.h"

using namespace Ast;
using namespace Codegen;

static Result_t callConstructor(CodegenInstance* cgi, TypePair_t* tp, FuncCall* fc)
{
	assert(tp);
	llvm::Value* ai = cgi->mainBuilder.CreateAlloca(tp->first, 0, "tmp");

	// TODO: constructor args
	std::vector<llvm::Value*> args;
	args.push_back(ai);
	for(Expr* e : fc->params)
		args.push_back(e->codegen(cgi).result.first);

	llvm::Function* initfunc = cgi->getStructInitialiser(fc, tp, args);

	cgi->mainBuilder.CreateCall(initfunc, args);
	llvm::Value* val = cgi->mainBuilder.CreateLoad(ai);

	return Result_t(val, ai);
}









Result_t FuncCall::codegen(CodegenInstance* cgi)
{
	// always try the type first.
	if(cgi->getType(this->name) != nullptr)
		return callConstructor(cgi, cgi->getType(this->name), this);

	FuncPair_t* fp = cgi->getDeclaredFunc(this);
	if(!fp)
		GenError::unknownSymbol(this, this->name, SymbolType::Function);

	llvm::Function* target = fp->first;
	if((target->arg_size() != this->params.size() && !target->isVarArg()) || (target->isVarArg() && target->arg_size() > 0 && this->params.size() == 0))
		error(this, "Expected %ld arguments, but got %ld arguments instead", target->arg_size(), this->params.size());

	std::vector<llvm::Value*> args;


	for(Expr* e : this->params)
		args.push_back(e->codegen(cgi).result.first);

	auto arg_it = target->arg_begin();
	for(size_t i = 0; i < args.size() && arg_it != target->arg_end(); i++, arg_it++)
	{
		if(arg_it->getType()->isIntegerTy() && args[i]->getType()->isIntegerTy())
		{
			args[i] = cgi->mainBuilder.CreateIntCast(args[i], arg_it->getType(), false);
		}
	}

	return Result_t(cgi->mainBuilder.CreateCall(target, args), 0);
}
















