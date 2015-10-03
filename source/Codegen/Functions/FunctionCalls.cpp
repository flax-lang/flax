// FuncCallCodegen.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "ast.h"
#include "codegen.h"

#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"

using namespace Ast;
using namespace Codegen;

Result_t CodegenInstance::callTypeInitialiser(TypePair_t* tp, Expr* user, std::vector<llvm::Value*> args)
{
	iceAssert(tp);
	llvm::Value* ai = this->allocateInstanceInBlock(tp->first, "tmp");

	args.insert(args.begin(), ai);

	llvm::Function* initfunc = this->getStructInitialiser(user, tp, args);

	this->builder.CreateCall(initfunc, args);
	llvm::Value* val = this->builder.CreateLoad(ai);

	return Result_t(val, ai);
}

Result_t FuncCall::codegen(CodegenInstance* cgi, llvm::Value* lhsPtr, llvm::Value* rhs)
{
	// always try the type first.
	if(TypePair_t* tp = cgi->getType(this->name))
	{
		std::vector<llvm::Value*> args;
		for(Expr* e : this->params)
			args.push_back(e->codegen(cgi).result.first);

		return cgi->callTypeInitialiser(tp, this, args);
	}
	else if(TypePair_t* tp = cgi->getType(cgi->mangleRawNamespace(this->name)))
	{
		std::vector<llvm::Value*> args;
		for(Expr* e : this->params)
			args.push_back(e->codegen(cgi).result.first);

		return cgi->callTypeInitialiser(tp, this, args);
	}




	std::vector<llvm::Value*> args;
	std::vector<llvm::Value*> argPtrs;


	llvm::Function* target = 0;
	if(this->cachedGenericFuncTarget == 0)
	{
		if(!this->cachedResolveTarget.resolved)
		{
			Resolved_t rt = cgi->resolveFunction(this, this->name, this->params);

			if(!rt.resolved)
			{
				// print a better error message.
				std::vector<std::string> argtypes;
				for(auto a : this->params)
					argtypes.push_back(cgi->getReadableType(a).c_str());

				std::string argstr;
				for(auto s : argtypes)
					argstr += ", " + s;

				if(argstr.length() > 0)
					argstr = argstr.substr(2);

				std::string candidates;
				for(auto fs : cgi->resolveFunctionName(this->name))
				{
					if(fs.second)
						candidates += cgi->printAst(fs.second) + "\n";
				}

				error(cgi, this, "No such function '%s' taking parameters (%s)\nPossible candidates:\n%s",
					this->name.c_str(), argstr.c_str(), candidates.c_str());
			}

			this->cachedResolveTarget = rt;
		}

		target = this->cachedResolveTarget.t.first;
		this->cachedResolveTarget.resolved = false;
	}
	else
	{
		target = this->cachedGenericFuncTarget;
	}

	iceAssert(target);
	bool checkVarArg = target->isVarArg();

	if((target->arg_size() != this->params.size() && !checkVarArg) || (checkVarArg && target->arg_size() > 0 && this->params.size() == 0))
	{
		error(cgi, this, "Expected %ld arguments, but got %ld arguments instead", target->arg_size(), this->params.size());
	}


	int argNum = 0;
	for(Expr* e : this->params)
	{
		ValPtr_t res = e->codegen(cgi).result;
		llvm::Value* arg = res.first;

		if(arg == nullptr || arg->getType()->isVoidTy())
			GenError::nullValue(cgi, this, argNum);

		if(checkVarArg && arg->getType()->isStructTy())
		{
			llvm::StructType* st = llvm::cast<llvm::StructType>(arg->getType());
			if(!st->isLiteral() && st->getStructName() != "String")
			{
				warn(cgi, e, "Passing structs to vararg functions can have unexpected results.");
			}
			else if(!st->isLiteral() && st->getStructName() == "String")
			{
				// this function knows what to do.
				cgi->autoCastType(llvm::Type::getInt8PtrTy(cgi->getContext()), arg, res.second);
			}
		}

		args.push_back(arg);
		argPtrs.push_back(res.second);
		argNum++;
	}




	auto arg_it = target->arg_begin();
	for(size_t i = 0; i < args.size() && arg_it != target->arg_end(); i++, arg_it++)
	{
		if(arg_it->getType() != args[i]->getType())
			cgi->autoCastType(arg_it, args[i], argPtrs[i]);

		if(arg_it->getType() != args[i]->getType())
		{
			error(cgi, this, "Argument %zu of function call is mismatched; expected '%s', got '%s'", i + 1,
				cgi->getReadableType(arg_it).c_str(), cgi->getReadableType(args[i]).c_str());
		}
	}

	// might not be a good thing to always do.
	// TODO: check this.
	// makes sure we call the function in our own module, because llvm only allows that.

	printf("target: %s\n", target->getName().bytes_begin());
	target = cgi->module->getFunction(target->getName());
	iceAssert(target);

	return Result_t(cgi->builder.CreateCall(target, args), 0);
}
















