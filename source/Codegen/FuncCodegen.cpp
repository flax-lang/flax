// FuncCodegen.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "../include/ast.h"
#include "../include/codegen.h"
#include "../include/llvm_all.h"

using namespace Ast;
using namespace Codegen;


#define OPTIMISE 0

Result_t FuncCall::codegen(CodegenInstance* cgi)
{
	FuncPair_t* fp = cgi->getDeclaredFunc(this->name);
	if(!fp)
		fp = cgi->getDeclaredFunc(cgi->mangleName(this->name, this->params));

	if(!fp)
		error(this, "Unknown function '%s' (mangled: %s)", this->name.c_str(), cgi->mangleName(this->name, this->params).c_str());

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

Result_t FuncDecl::codegen(CodegenInstance* cgi)
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
	llvm::GlobalValue::LinkageTypes linkageType;

	if(this->isFFI)
		linkageType = llvm::Function::ExternalWeakLinkage;

	else if((this->attribs & Attr_VisPrivate) || (this->attribs & Attr_VisInternal))
		linkageType = llvm::Function::InternalLinkage;

	else
		linkageType = llvm::Function::ExternalLinkage;


	llvm::Function* func = llvm::Function::Create(ft, linkageType, this->mangledName, cgi->mainModule);


	// check for redef
	if(func->getName() != this->mangledName)
	{
		if(!this->isFFI)
		{
			error(this, "Redefinition of function '%s'", this->name.c_str());
		}
		else
		{
			// check for same name but different args
			// TODO: c++ compat
		}
	}

	cgi->addFunctionToScope(this->mangledName, FuncPair_t(func, this));

	if(this->attribs & Attr_VisPublic)
		cgi->getRootAST()->publicFuncs.push_back(std::pair<FuncDecl*, llvm::Function*>(this, func));

	return Result_t(func, 0);
}

Result_t ForeignFuncDecl::codegen(CodegenInstance* cgi)
{
	return this->decl->codegen(cgi);
}

Result_t Closure::codegen(CodegenInstance* cgi)
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





Result_t Func::codegen(CodegenInstance* cgi)
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
	llvm::Value* lastVal = this->closure->codegen(cgi).result.first;

	// check if we're not returning void
	if(cgi->determineVarType(this) != VarType::Void)
	{
		// if we have no statements at all:
		if(this->closure->statements.size() == 0)
			error(this, "Return value required for function '%s'", this->decl->name.c_str());

		// TODO: proper detection of whether all code paths return
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

	if(prevBlock)
		cgi->mainBuilder.SetInsertPoint(prevBlock);

	return Result_t(func, 0);
}














