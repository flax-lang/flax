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

	bool broke = false;
	for(Expr* e : this->statements)
	{
		if(e->isBreaking() && cgi->getCurrentFunctionScope()->block->deferredStatements.size() > 0)
		{
			for(Expr* e : cgi->getCurrentFunctionScope()->block->deferredStatements)
			{
				e->codegen(cgi);
			}
		}

		if(!broke)
		{
			lastval = e->codegen(cgi);
		}

		if(lastval.type == ResultType::BreakCodegen)
			broke = true;		// don't generate the rest of the code. cascade the BreakCodegen value into higher levels
	}

	return lastval;
}

Result_t Func::codegen(CodegenInstance* cgi, llvm::Value* lhsPtr, llvm::Value* rhs)
{
	if(this->decl->genericTypes.size() > 0)
	{
		return Result_t(0, 0);
	}



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
	cgi->setCurrentFunctionScope(this);


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


	// since Flax is a statically typed language (good!)
	// we know the types of everything at compilet time

	// to work around things like dangling returns (ie. if { return } else { return }, and the function itself has a return)
	// we verify that all the code paths return first
	// verifyAllCodePathsReturn also gives us the number of statements before everything is guaranteed
	// to return, so we cut out anything after that, and issue a warning.


	// check if we're not returning void
	bool isImplicitReturn = false;
	bool doRetVoid = false;
	bool premature = false;
	if(this->decl->type.strType != "Void")
	{
		size_t counter = 0;
		isImplicitReturn = cgi->verifyAllPathsReturn(this, &counter, false);


		if(counter != this->block->statements.size())
		{
			warn(cgi, this->block->statements[counter], "Code will never be executed");

			// cut off the rest.
			this->block->statements.erase(this->block->statements.begin() + counter, this->block->statements.end());
			premature = true;
		}
	}
	else
	{
		// if the last expression was a return, don't add an explicit one.
		if(this->block->statements.size() == 0
			|| (this->block->statements.size() > 0 && !dynamic_cast<Return*>(this->block->statements.back())))
		{
			doRetVoid = true;
		}
	}





	// codegen everything in the body.
	Result_t lastval = this->block->codegen(cgi);


	// verify again, this type checking the types
	cgi->verifyAllPathsReturn(this, nullptr, true);

	if(doRetVoid)
		cgi->mainBuilder.CreateRetVoid();

	else if(isImplicitReturn)
		cgi->mainBuilder.CreateRet(lastval.result.first);


	llvm::verifyFunction(*func, &llvm::errs());
	cgi->Fpm->run(*func);

	// we've codegen'ed that stuff, pop the symbol table
	cgi->popScope();

	if(prevBlock)
		cgi->mainBuilder.SetInsertPoint(prevBlock);

	cgi->clearCurrentFunctionScope();
	return Result_t(func, 0);
}














