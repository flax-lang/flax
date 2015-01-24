// FuncCodegen.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "../include/ast.h"
#include "../include/codegen.h"
#include "../include/llvm_all.h"

using namespace Ast;
using namespace Codegen;


#define OPTIMISE 1

ValPtr_p FuncCall::codegen(CodegenInstance* cgi)
{
	llvm::Function* target = cgi->mainModule->getFunction(this->name);
	if(!target)
		target = cgi->mainModule->getFunction(cgi->mangleName(this->name, this->params));

	if(target == 0)
		error(this, "Unknown function '%s'", this->name.c_str());

	if((target->arg_size() != this->params.size() && !target->isVarArg()) || (target->isVarArg() && target->arg_size() > 0 && this->params.size() == 0))
		error(this, "Expected %ld arguments, but got %ld arguments instead", target->arg_size(), this->params.size());

	std::vector<llvm::Value*> args;

	// we need to get the function declaration
	FuncDecl* decl = cgi->getFuncDecl(this->name);
	if(decl)
	{
		for(int i = 0; i < this->params.size(); i++)
			this->params[i] = cgi->autoCastType(decl->params[i], this->params[i]);
	}

	for(Expr* e : this->params)
		args.push_back(e->codegen(cgi).first);

	return ValPtr_p(cgi->mainBuilder.CreateCall(target, args), 0);
}

ValPtr_p FuncDecl::codegen(CodegenInstance* cgi)
{
	std::vector<llvm::Type*> argtypes;
	std::deque<Expr*> params_expr;
	for(VarDecl* v : this->params)
	{
		params_expr.push_back(v);
		argtypes.push_back(cgi->getLlvmType(v));
	}

	// check if empty and if it's an extern. mangle the name to include type info if possible.
	this->mangledName = this->name;
	if((!this->isFFI || this->attribs & Attr_ForceMangle) && !(this->attribs & Attr_NoMangle))
		this->mangledName = cgi->mangleName(this->name, params_expr);

	llvm::FunctionType* ft = llvm::FunctionType::get(cgi->getLlvmType(this), argtypes, this->hasVarArg);
	llvm::Function* func = llvm::Function::Create(ft, (this->attribs & Attr_VisPublic || this->isFFI) ? llvm::Function::ExternalLinkage : llvm::Function::InternalLinkage, this->mangledName, cgi->mainModule);

	// check for redef
	if(func->getName() != this->mangledName)
		error(this, "Redefinition of function '%s'", this->name.c_str());

	cgi->getVisibleFuncDecls()[this->mangledName] = FuncPair_t(func, this);

	if(this->attribs & Attr_VisPublic)
		cgi->getRootAST()->publicFuncs.push_back(std::pair<FuncDecl*, llvm::Function*>(this, func));

	return ValPtr_p(func, 0);
}

ValPtr_p ForeignFuncDecl::codegen(CodegenInstance* cgi)
{
	return this->decl->codegen(cgi);
}

ValPtr_p Closure::codegen(CodegenInstance* cgi)
{
	ValPtr_p lastVal;
	for(Expr* e : this->statements)
		lastVal = e->codegen(cgi);

	return lastVal;
}





ValPtr_p Func::codegen(CodegenInstance* cgi)
{
	// because the main code generator is two-pass, we expect all function declarations to have been generated
	// so just fetch it.

	llvm::Function* func = cgi->mainModule->getFunction(this->decl->mangledName);
	if(!func)
	{
		error("(%s:%s:%d) -> Internal check failed: Failed to get function declaration for func '%s'", __FILE__, __PRETTY_FUNCTION__, __LINE__, this->decl->name.c_str());
		return ValPtr_p(0, 0);
	}

	// we need to clear all previous blocks' symbols
	// but we can't destroy them, so employ a stack method.
	// create a new 'table' for our own usage
	cgi->pushScope();

	llvm::BasicBlock* block = llvm::BasicBlock::Create(cgi->getContext(), "entry", func);
	cgi->mainBuilder.SetInsertPoint(block);


	// unfortunately, because we have to clear the symtab above, we need to add the param vars here
	int i = 0;
	for(llvm::Function::arg_iterator it = func->arg_begin(); i != func->arg_size(); it++, i++)
	{
		it->setName(this->decl->params[i]->name);

		llvm::AllocaInst* ai = cgi->allocateInstanceInBlock(func, this->decl->params[i]);
		cgi->mainBuilder.CreateStore(it, ai);

		cgi->getSymTab()[this->decl->params[i]->name] = std::pair<llvm::AllocaInst*, VarDecl*>(ai, this->decl->params[i]);
	}


	// codegen everything in the body.
	llvm::Value* lastVal = this->closure->codegen(cgi).first;

	// check if we're not returning void
	if(cgi->determineVarType(this) != VarType::Void)
	{
		if(this->closure->statements.size() == 0)
			error(this, "Return value required for function '%s'", this->decl->name.c_str());

		// the last expr is the final return value.
		// if we had an explicit return, then the dynamic cast will succeed and we don't need to do anything
		if(!dynamic_cast<Return*>(this->closure->statements.back()))
		{
			// else, if the cast failed it means we didn't explicitly return, so we take the
			// value of the last expr as the return value.

			// actually, if the last statement is an if-clause, then we can't tell for sure
			// this will abort at runtime.

			// TODO: make better.
			if(!dynamic_cast<If*>(this->closure->statements.back()))
				cgi->mainBuilder.CreateRet(lastVal);
		}
	}
	else
	{
		cgi->mainBuilder.CreateRetVoid();
	}

	llvm::verifyFunction(*func, &llvm::errs());

	if(OPTIMISE)
		cgi->Fpm->run(*func);


	// we've codegen'ed that stuff, pop the symbol table
	cgi->popScope();

	return ValPtr_p(func, 0);
}














