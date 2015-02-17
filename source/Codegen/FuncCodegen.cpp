// FuncCodegen.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "../include/ast.h"
#include "../include/codegen.h"
#include "../include/llvm_all.h"

using namespace Ast;
using namespace Codegen;

Result_t BracedBlock::codegen(CodegenInstance* cgi, llvm::Value* lhsPtr, llvm::Value* rhs)
{
	Result_t lastval(0, 0);
	for(Expr* e : this->statements)
	{
		lastval = e->codegen(cgi);

		if(lastval.type == ResultType::BreakCodegen)
			break;		// haha: don't generate the rest of the code. cascade the BreakCodegen value into higher levels
	}

	return lastval;
}

Result_t Func::codegen(CodegenInstance* cgi, llvm::Value* lhsPtr, llvm::Value* rhs)
{
	// because the main code generator is two-pass, we expect all function declarations to have been generated
	// so just fetch it.

	llvm::Function* func = cgi->mainModule->getFunction(this->decl->mangledName);
	if(!func)
	{
		this->decl->codegen(cgi);
		return this->codegen(cgi);
	}

	// we need to clear all previous blocks' symbols
	// but we can't destroy them, so employ a stack method.
	// create a new 'table' for our own usage.
	// the reverse stack searching for symbols makes sure we can reference variables in outer scopes
	cgi->pushScope();

	// to support declaring functions inside functions, we need to remember
	// the previous insert point, or all further codegen will happen inside this function
	// and fuck shit up big time
	llvm::BasicBlock* prevBlock = cgi->mainBuilder.GetInsertBlock();

	llvm::BasicBlock* block = llvm::BasicBlock::Create(cgi->getContext(), this->decl->name + "_entry", func);
	cgi->mainBuilder.SetInsertPoint(block);


	// unfortunately, because we have to clear the symtab above, we need to add the param vars here
	if(func->arg_size() > 0)
	{
		int i = 0;
		for(llvm::Function::arg_iterator it = func->arg_begin(); it != func->arg_end(); it++, i++)
		{
			it->setName(this->decl->params[i]->name);

			llvm::AllocaInst* ai = cgi->allocateInstanceInBlock(this->decl->params[i]);
			cgi->mainBuilder.CreateStore(it, ai);

			cgi->addSymbol(this->decl->params[i]->name, ai, this->decl->params[i]);
		}
	}


	// codegen everything in the body.
	Result_t lastval = this->block->codegen(cgi);

	// check if we're not returning void
	bool isImplicitReturn = false;
	if(this->decl->type != "Void")
	{
		isImplicitReturn = cgi->verifyAllPathsReturn(this);
	}
	else
	{
		// if the last expression was a return, don't add an explicit one.
		if(this->block->statements.size() > 0 && !dynamic_cast<Return*>(this->block->statements.back()))
			cgi->mainBuilder.CreateRetVoid();
	}

	if(isImplicitReturn)
		cgi->mainBuilder.CreateRet(lastval.result.first);

	llvm::verifyFunction(*func, &llvm::errs());
	cgi->Fpm->run(*func);

	// we've codegen'ed that stuff, pop the symbol table
	cgi->popScope();

	if(prevBlock)
		cgi->mainBuilder.SetInsertPoint(prevBlock);

	return Result_t(func, 0);
}














