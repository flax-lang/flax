// Function.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "pts.h"
#include "ast.h"
#include "codegen.h"
#include <algorithm>

using namespace Ast;
using namespace Codegen;

Result_t Func::codegen(CodegenInstance* cgi, fir::Type* extratype, fir::Value* target)
{
	static bool didRecurse = false;

	if(this->didCodegen && !target)
		error(this, "Tried to generate function twice (%s)", this->decl->ident.str().c_str());

	this->didCodegen = true;



	bool isGeneric = this->decl->genericTypes.size() > 0;

	if(isGeneric && !target)
	{
		FunctionTree* cft = cgi->getCurrentFuncTree();
		iceAssert(cft);

		// we should already be inside somewhere
		auto it = std::find(cft->genericFunctions.begin(), cft->genericFunctions.end(), std::make_pair(this->decl, this));
		if(it == cft->genericFunctions.end())
		{
			error(this, "%s", cft->nsName.c_str());
		}

		// toplevel already added us.
		// cft->genericFunctions.push_back(std::make_pair(this->decl, this));
	}



	fir::Function* func = 0;
	if(isGeneric && target != 0)
	{
		iceAssert(func = dynamic_cast<fir::Function*>(target));
	}
	else
	{
		// guaranteed to exist eventually, because recursion (below)
		func = this->decl->generatedFunc;

		if(!func)
		{
			this->didCodegen = false;
			if(isGeneric)
			{
				// our primary purpose was to add the generic function to the functree
				// after that our job is done (clearly we can't generate a generic function without knowing the type parameters)

				return Result_t(0, 0);
			}
			else
			{
				if(didRecurse) error(this, "Failed to generate function %s", this->decl->ident.str().c_str());

				// generate it.
				this->decl->codegen(cgi);
				this->didCodegen = false;

				didRecurse = true;
				return this->codegen(cgi);	// recursively.
			}
		}

		didRecurse = false;
	}

	if(this->block == 0)
		error(this, "Function needs a body in this context");


	iceAssert(func);
	if(func->wasDeclaredWithBodyElsewhere())
		error(this->decl, "Function '%s' was already declared with a body in another, imported, module", this->decl->ident.name.c_str());


	// we need to clear all previous blocks' symbols
	// but we can't destroy them, so employ a stack method.
	// create a new 'table' for our own usage.
	// the reverse stack searching for symbols makes sure we can reference variables in outer scopes, but at the same time
	// we can shadow outer variables with our own.
	cgi->pushScope();
	cgi->pushNamespaceScope(this->decl->ident.str());
	cgi->setCurrentFunctionScope(this);



	// to support declaring functions inside functions, we need to remember
	// the previous insert point, or all further codegen will happen inside this function
	// and fuck shit up big time
	fir::IRBlock* prevBlock = cgi->irb.getCurrentBlock();

	fir::IRBlock* irblock = cgi->irb.addNewBlockInFunction(this->decl->ident.name + "_entry", func);
	cgi->irb.setCurrentBlock(irblock);



	std::vector<VarDecl*> vprs = this->decl->params;
	if(this->decl->params.size() + 1 == func->getArgumentCount())
	{
		// we need to add the self param.
		iceAssert(this->decl->parentClass && this->decl->parentClass->createdType);

		VarDecl* fake = new VarDecl(this->decl->pin, "self", "");
		fake->ptype = new pts::Type(this->decl->parentClass->createdType->getPointerTo());

		vprs.insert(vprs.begin(), fake);
	}


	for(size_t i = 0; i < vprs.size(); i++)
	{
		func->getArguments()[i]->setName(vprs[i]->ident);

		if(!isGeneric)
		{
			if(func->getArguments()[i]->getType() != vprs[i]->getType(cgi))
			{
				error(this, "Mismatched types: '%s' vs '%s'", func->getArguments()[i]->getType()->str().c_str(),
					vprs[i]->getType(cgi)->str().c_str());
			}
		}

		fir::Value* ai = 0;
		if(!vprs[i]->immutable)
		{
			ai = cgi->getStackAlloc(func->getArguments()[i]->getType());
			cgi->irb.CreateStore(func->getArguments()[i], ai);
		}
		else
		{
			ai = cgi->getImmutStackAllocValue(func->getArguments()[i]);
		}

		cgi->addSymbol(vprs[i]->ident.name, ai, vprs[i]);
		func->getArguments()[i]->setValue(ai);
	}


	// since Flax is a statically typed language (good!)
	// we know the types of everything at compile time

	// to work around things like dangling returns (ie. if { return } else { return }, and the function itself has a return)
	// we verify that all the code paths return first
	// verifyAllCodePathsReturn also gives us the number of statements before everything is guaranteed
	// to return, so we cut out anything after that, and issue a warning.


	// check if we're not returning void
	bool isImplicitReturn = false;
	bool doRetVoid = false;

	if(this->decl->ptype->str() != VOID_TYPE_STRING)
	{
		size_t counter = 0;
		isImplicitReturn = cgi->verifyAllPathsReturn(this, &counter, false, func->getReturnType());


		if(counter != this->block->statements.size())
		{
			warn(this->block->statements[counter], "Code will never be executed");

			// cut off the rest.
			this->block->statements.erase(this->block->statements.begin() + counter, this->block->statements.end());
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
	fir::Value* value = 0;
	fir::Value* pointer = 0;
	ValueKind vkind;

	std::tie(value, pointer, vkind) = this->block->codegen(cgi);

	// verify again, this type checking the types
	bool needImplicit = cgi->verifyAllPathsReturn(this, nullptr, true, func->getReturnType());

	if(doRetVoid)
	{
		cgi->irb.CreateReturnVoid();
	}
	else if(isImplicitReturn)
	{
		fir::Type* needed = func->getReturnType();

		if(value->getType() != needed)
		{
			value = cgi->autoCastType(func->getReturnType(), value, pointer);
			if(value->getType() != needed)
			{
				error(this->block, "Return type mismatch: needed '%s', got '%s'", needed->str().c_str(), value->getType()->str().c_str());
			}
		}

		// if it's an rvalue, we make a new one, increment its refcount
		if(cgi->isRefCountedType(value->getType()))
			cgi->incrementRefCount(value);

		cgi->irb.CreateReturn(value);
	}
	else if(needImplicit)
	{
		error(this->block->statements.size() > 0 ? this->block->statements.back() : this->decl,
			"Function '%s' has a return type of '%s', but did not return anything; implicit return only valid if function body contains exactly "
			"1 expression, found %zu here", this->decl->ident.name.c_str(), func->getReturnType()->str().c_str(),
			this->block->statements.size());
	}


	// cgi->restoreScope(s);
	cgi->popScope();

	if(prevBlock)
		cgi->irb.setCurrentBlock(prevBlock);

	cgi->popNamespaceScope();
	cgi->clearCurrentFunctionScope();
	return Result_t(func, 0);
}



fir::Type* Func::getType(CodegenInstance* cgi, fir::Type* extratype, bool allowFail)
{
	return this->decl->getType(cgi);
}











