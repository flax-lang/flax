// FuncCodegen.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "../include/ast.h"
#include "../include/codegen.h"
#include "../include/llvm_all.h"

using namespace Ast;
using namespace Codegen;


#define OPTIMISE 0

ValPtr_p FuncCall::codeGen()
{
	llvm::Function* target = mainModule->getFunction(this->name);
	if(!target)
		target = mainModule->getFunction(mangleName(this->name, this->params));

	if(target == 0)
		error("Unknown function '%s'", this->name.c_str());


	if(target->arg_size() != this->params.size())
		error("Expected %ld arguments, but got %ld arguments instead", target->arg_size(), this->params.size());

	std::vector<llvm::Value*> args;

	// we need to get the function declaration
	FuncDecl* decl = funcTable[this->name];
	assert(decl);

	for(int i = 0; i < this->params.size(); i++)
		this->params[i] = autoCastType(decl->params[i], this->params[i]);

	for(Expr* e : this->params)
	{
		args.push_back(e->codeGen().first);
		if(args.back() == nullptr)
			return ValPtr_p(0, 0);
	}

	return ValPtr_p(mainBuilder.CreateCall(target, args), 0);
}

ValPtr_p FuncDecl::codeGen()
{
	std::string mangledname;

	std::vector<llvm::Type*> argtypes;
	std::deque<Expr*> params_expr;
	for(VarDecl* v : this->params)
	{
		params_expr.push_back(v);
		argtypes.push_back(getLlvmType(v));
	}

	// check if empty and if it's an extern. mangle the name to include type info if possible.
	std::string mname = this->name;
	if(!mangledname.empty() && !this->isFFI)
		mname = mangleName(this->name, params_expr);

	llvm::FunctionType* ft = llvm::FunctionType::get(getLlvmType(this), argtypes, false);
	llvm::Function* func = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, mname, mainModule);

	// check for redef
	if(func->getName() != mname)
		error("Redefinition of function '%s'", this->name.c_str());

	funcTable[mname] = this;
	return ValPtr_p(func, 0);
}

ValPtr_p ForeignFuncDecl::codeGen()
{
	return this->decl->codeGen();
}

ValPtr_p Closure::codeGen()
{
	ValPtr_p lastVal;
	for(Expr* e : this->statements)
		lastVal = e->codeGen();

	return lastVal;
}





ValPtr_p Func::codeGen()
{
	// because the main code generator is two-pass, we expect all function declarations to have been generated
	// so just fetch it.

	llvm::Function* func = mainModule->getFunction(this->decl->name);
	if(!func)
	{
		error("(%s:%s:%d) -> Internal check failed: Failed to get function declaration for func '%s'", __FILE__, __PRETTY_FUNCTION__, __LINE__, this->decl->name.c_str());
		return ValPtr_p(0, 0);
	}

	// we need to clear all previous blocks' symbols
	// but we can't destroy them, so employ a stack method.
	// create a new 'table' for our own usage
	pushScope();

	llvm::BasicBlock* block = llvm::BasicBlock::Create(getContext(), "entry", func);
	mainBuilder.SetInsertPoint(block);


	// unfortunately, because we have to clear the symtab above, we need to add the param vars here
	int i = 0;
	for(llvm::Function::arg_iterator it = func->arg_begin(); i != func->arg_size(); it++, i++)
	{
		it->setName(this->decl->params[i]->name);

		llvm::AllocaInst* ai = allocateInstanceInBlock(func, this->decl->params[i]);
		mainBuilder.CreateStore(it, ai);

		getSymTab()[this->decl->params[i]->name] = std::pair<llvm::AllocaInst*, VarDecl*>(ai, this->decl->params[i]);
	}


	// codegen everything in the body.
	llvm::Value* lastVal = this->closure->codeGen().first;

	// check if we're not returning void
	if(this->decl->varType != VarType::Void)
	{
		if(this->closure->statements.size() == 0)
			error("Return value required for function '%s'", this->decl->name.c_str());

		// the last expr is the final return value.
		// if we had an explicit return, then the dynamic cast will succeed and we don't need to do anything
		if(!dynamic_cast<Return*>(this->closure->statements.back()))
		{
			// else, if the cast failed it means we didn't explicitly return, so we take the
			// value of the last expr as the return value.
			mainBuilder.CreateRet(lastVal);
		}
	}
	else
	{
		mainBuilder.CreateRetVoid();
	}

	llvm::verifyFunction(*func);

	if(OPTIMISE)
		Fpm->run(*func);


	// we've codegen'ed that stuff, pop the symbol table
	popScope();

	return ValPtr_p(func, 0);
}
