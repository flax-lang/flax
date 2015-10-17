// FuncCodegen.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "codegen.h"

#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"

using namespace Ast;
using namespace Codegen;

Result_t BracedBlock::codegen(CodegenInstance* cgi, fir::Value* lhsPtr, fir::Value* rhs)
{
	Result_t lastval(0, 0);
	cgi->pushScope();

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

	cgi->popScope();
	return lastval;
}

Result_t Func::codegen(CodegenInstance* cgi, fir::Value* lhsPtr, fir::Value* rhs)
{
	if(this->didCodegen)
		error(this, "Tried to generate function twice (%s)", this->decl->name.c_str());

	this->didCodegen = true;

	bool isPublic = this->decl->attribs & Attr_VisPublic;
	bool isGeneric = this->decl->genericTypes.size() > 0;

	// because the main code generator is two-pass, we expect all function declarations to have been generated
	// so just fetch it.

	if(isGeneric && isPublic)
	{
		cgi->rootNode->publicGenericFunctions.push_back(std::make_pair(this->decl, this));
	}

	fir::Function* func = 0;

	if(isGeneric && lhsPtr != 0)
	{
		iceAssert(func = dynamic_cast<fir::Function*>(lhsPtr));
	}
	else
	{
		func = cgi->module->getFunction(this->decl->mangledName);
		if(!func)
		{
			if(isGeneric && !isPublic)
			{
				warn(this, "Function %s is never called (%s)", this->decl->name.c_str(), this->decl->mangledName.c_str());
				return Result_t(0, 0);
			}
			else if(!isGeneric && !isPublic)
			{
				// this should not happen
				// warn(this, "Function %s did not have a declaration, skipping...", this->decl->name.c_str());
				warn(this, "Function %s is not public and is never called; it will not be emitted", this->decl->name.c_str());
				return Result_t(0, 0);
			}
			else
			{
				// generate it.
				this->decl->codegen(cgi);
				this->didCodegen = false;

				return this->codegen(cgi);	// recursively.
			}
		}
	}



	// we need to clear all previous blocks' symbols
	// but we can't destroy them, so employ a stack method.
	// create a new 'table' for our own usage.
	// the reverse stack searching for symbols makes sure we can reference variables in outer scopes, but at the same time
	// we can shadow outer variables with our own.
	cgi->pushScope();
	cgi->setCurrentFunctionScope(this);


	// to support declaring functions inside functions, we need to remember
	// the previous insert point, or all further codegen will happen inside this function
	// and fuck shit up big time
	fir::IRBlock* prevBlock = cgi->builder.getCurrentBlock();

	fir::IRBlock* block = cgi->builder.addNewBlockInFunction(this->decl->name + "_entry", func);
	cgi->builder.setCurrentBlock(block);



	// unfortunately, because we have to clear the symtab above, we need to add the param vars here
	for(size_t i = 0; i < func->getArgumentCount(); i++)
	{
		func->getArguments()[i]->setName(this->decl->params[i]->name);

		fir::Value* ai = 0;

		if(isGeneric)
		{
			ai = cgi->allocateInstanceInBlock(this->decl->instantiatedGenericTypes[i]);
		}
		else
		{
			ai = cgi->allocateInstanceInBlock(this->decl->params[i]);
		}

		cgi->builder.CreateStore(func->getArguments()[i], ai);
		cgi->addSymbol(this->decl->params[i]->name, ai, this->decl->params[i]);
		func->getArguments()[i]->setValue(ai);
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
	// bool premature = false;
	if(this->decl->type.strType != "Void")
	{
		size_t counter = 0;
		isImplicitReturn = cgi->verifyAllPathsReturn(this, &counter, false, isGeneric ? this->decl->instantiatedGenericReturnType : 0);


		if(counter != this->block->statements.size())
		{
			warn(this->block->statements[counter], "Code will never be executed");

			// cut off the rest.
			this->block->statements.erase(this->block->statements.begin() + counter, this->block->statements.end());
			// premature = true;
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
	cgi->verifyAllPathsReturn(this, nullptr, true, isGeneric ? this->decl->instantiatedGenericReturnType : 0);

	if(doRetVoid)
		cgi->builder.CreateReturnVoid();

	else if(isImplicitReturn)
		cgi->builder.CreateReturn(lastval.result.first);


	// todo: optimise/run llvm passes
	// fir::verifyFunction(*func, &fir::errs());
	// cgi->Fpm->run(*func);

	// we've codegen'ed that stuff, pop the symbol table
	cgi->popScope();

	if(prevBlock)
		cgi->builder.setCurrentBlock(prevBlock);

	cgi->clearCurrentFunctionScope();
	return Result_t(func, 0);
}














