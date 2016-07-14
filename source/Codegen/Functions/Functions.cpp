// FuncCodegen.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "codegen.h"

using namespace Ast;
using namespace Codegen;

Result_t BracedBlock::codegen(CodegenInstance* cgi, fir::Value* extra)
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

Result_t Func::codegen(CodegenInstance* cgi, fir::Value* extra)
{
	static bool didRecurse = false;

	if(this->didCodegen && !extra)
		error(this, "Tried to generate function twice (%s)", this->decl->ident.str().c_str());

	this->didCodegen = true;

	// bool isPublic = this->decl->attribs & Attr_VisPublic;
	bool isGeneric = this->decl->genericTypes.size() > 0;


	if(isGeneric)
	{
		FunctionTree* cft = cgi->getCurrentFuncTree();
		iceAssert(cft);

		cft->genericFunctions.push_back(std::make_pair(this->decl, this));
	}

	fir::Function* func = 0;

	if(isGeneric && extra != 0)
	{
		iceAssert(func = dynamic_cast<fir::Function*>(extra));
	}
	else
	{
		bool notMangling = (this->decl->attribs & Attr_NoMangle || this->decl->isFFI);
		func = notMangling ? cgi->module->getFunction(Identifier(this->decl->ident.name, IdKind::Name))
							: cgi->module->getFunction(this->decl->ident);

		if(!func)
		{
			this->didCodegen = false;
			if(isGeneric)
			{
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

	fir::IRBlock* irblock = cgi->builder.addNewBlockInFunction(this->decl->ident.name + "_entry", func);
	cgi->builder.setCurrentBlock(irblock);



	std::deque<VarDecl*> vprs = decl->params;
	if(this->decl->params.size() + 1 == func->getArgumentCount())
	{
		// we need to add the self param.
		iceAssert(this->decl->parentClass && this->decl->parentClass->createdType);

		VarDecl* fake = new VarDecl(this->decl->pin, "self", "");
		fake->type.ftype = this->decl->parentClass->createdType->getPointerTo();

		vprs.push_front(fake);
	}

	for(size_t i = 0; i < vprs.size(); i++)
	{
		func->getArguments()[i]->setName(vprs[i]->ident);

		if(isGeneric)
		{
			iceAssert(func->getArguments()[i]->getType() == this->decl->instantiatedGenericTypes[i]);
		}
		else
		{
			iceAssert(func->getArguments()[i]->getType() == cgi->getExprType(vprs[i]));
		}

		fir::Value* ai = 0;
		if(!vprs[i]->immutable)
		{
			ai = cgi->getStackAlloc(func->getArguments()[i]->getType());
			cgi->builder.CreateStore(func->getArguments()[i], ai);
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
	// bool premature = false;
	if(this->decl->type.strType != VOID_TYPE_STRING)
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
	{
		fir::Type* needed = func->getReturnType();
		if(lastval.result.first->getType() != needed)
			lastval.result.first = cgi->autoCastType(func->getReturnType(), lastval.result.first, lastval.result.second);

		cgi->builder.CreateReturn(lastval.result.first);
	}


	// we've codegen'ed that stuff, pop the symbol table
	cgi->popScope();

	if(prevBlock)
		cgi->builder.setCurrentBlock(prevBlock);

	cgi->clearCurrentFunctionScope();
	return Result_t(func, 0);
}














