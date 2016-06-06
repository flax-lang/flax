// FuncCallCodegen.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "ast.h"
#include "codegen.h"

using namespace Ast;
using namespace Codegen;

Result_t CodegenInstance::callTypeInitialiser(TypePair_t* tp, Expr* user, std::vector<fir::Value*> args)
{
	iceAssert(tp);
	fir::Value* ai = this->allocateInstanceInBlock(tp->first, "tmp");

	args.insert(args.begin(), ai);

	fir::Function* initfunc = this->getStructInitialiser(user, tp, args);

	this->builder.CreateCall(initfunc, args);
	fir::Value* val = this->builder.CreateLoad(ai);

	return Result_t(val, ai);
}

Result_t FuncCall::codegen(CodegenInstance* cgi, fir::Value* lhsPtr, fir::Value* rhs)
{
	// always try the type first.
	if(TypePair_t* tp = cgi->getType(this->name))
	{
		std::vector<fir::Value*> args;
		for(Expr* e : this->params)
			args.push_back(e->codegen(cgi).result.first);

		return cgi->callTypeInitialiser(tp, this, args);
	}
	else if(TypePair_t* tp = cgi->getType(cgi->mangleRawNamespace(this->name)))
	{
		std::vector<fir::Value*> args;
		for(Expr* e : this->params)
			args.push_back(e->codegen(cgi).result.first);

		return cgi->callTypeInitialiser(tp, this, args);
	}

	std::vector<fir::Value*> args;
	std::vector<fir::Value*> argPtrs;

	fir::Function* target = 0;
	if(this->cachedGenericFuncTarget == 0)
	{
		// we're not a generic function.
		if(!this->cachedResolveTarget.resolved)
		{
			Resolved_t rt = cgi->resolveFunction(this, this->name, this->params);

			if(!rt.resolved)
			{
				// todo. do generic.
				this->cachedGenericFuncTarget = cgi->tryResolveAndInstantiateGenericFunction(this);
				if(this->cachedGenericFuncTarget != 0)
				{
					rt.resolved = true;
					rt.t.first = this->cachedGenericFuncTarget;
					target = this->cachedGenericFuncTarget;
				}
			}

			if(!rt.resolved && !target)
			{
				failedToFind:

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
				std::deque<FuncPair_t> reses;

				for(auto fs : reses = cgi->resolveFunctionName(this->name))
				{
					if(fs.second)
						candidates += cgi->printAst(fs.second) + "\n";
				}

				error(this, "No such function '%s' taking parameters (%s)\nPossible candidates (%zu):\n%s",
					this->name.c_str(), argstr.c_str(), reses.size(), candidates.c_str());
			}

			if(rt.t.first == 0)
			{
				// generate it.
				rt.t.second->codegen(cgi);

				// printf("expediting function call to %s\n", this->name.c_str());

				rt = cgi->resolveFunction(this, this->name, this->params);
				if(!rt.resolved) error("nani???");
				if(rt.t.first == 0) goto failedToFind;
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
	bool checkCVarArg = target->isCStyleVarArg();
	bool checkVariadic = target->isVariadic();

	bool isAnyKindOfVariadic = checkCVarArg || checkVariadic;

	if((target->getArgumentCount() != this->params.size() && !isAnyKindOfVariadic)
		|| (isAnyKindOfVariadic && target->getArgumentCount() > 0 && this->params.size() == 0))
	{
		error(this, "Expected %ld arguments, but got %ld arguments instead (how did this get through?)", target->getArgumentCount(),
			this->params.size());
	}


	if(!checkVariadic)
	{
		int argNum = 0;
		for(Expr* e : this->params)
		{
			ValPtr_t res = e->codegen(cgi).result;
			fir::Value* arg = res.first;

			if(arg == nullptr || arg->getType()->isVoidType())
				GenError::nullValue(cgi, this, argNum);

			if(checkCVarArg && arg->getType()->isStructType())
			{
				fir::StructType* st = arg->getType()->toStructType();
				if(!st->isLiteralStruct() && st->getStructName() != "String")
				{
					warn(e, "Passing structs to C-style variadic functions can have unexpected results.");
				}
				else if(!st->isLiteralStruct() && st->getStructName() == "String")
				{
					// this function knows what to do.
					arg = cgi->autoCastType(fir::PointerType::getInt8Ptr(cgi->getContext()), arg, res.second);
				}
			}

			args.push_back(arg);
			argPtrs.push_back(res.second);
			argNum++;
		}


		for(size_t i = 0; i < std::min(args.size(), target->getArgumentCount()); i++)
		{
			if(target->getArguments()[i]->getType() != args[i]->getType())
				args[i] = cgi->autoCastType(target->getArguments()[i], args[i], argPtrs[i]);

			if(target->getArguments()[i]->getType() != args[i]->getType())
			{
				error(this, "Argument %zu of function call is mismatched; expected '%s', got '%s'", i + 1,
					target->getArguments()[i]->getType()->str().c_str(), args[i]->getType()->str().c_str());
			}
		}
	}
	else
	{
		// variadic.
		// remember, last argument is the llarray.
		// do until the penultimate argument.
		for(size_t i = 0; i < target->getArgumentCount() - 1; i++)
		{
			Expr* ex = params[i];

			ValPtr_t res = ex->codegen(cgi).result;
			fir::Value* arg = res.first;

			if(arg == nullptr || arg->getType()->isVoidType())
				GenError::nullValue(cgi, this, i);

			args.push_back(arg);
			argPtrs.push_back(res.second);
		}

		// do the last.
		fir::Type* variadicType = target->getArguments().back()->getType()->toLLVariableArray()->getElementType();
		std::deque<fir::Value*> variadics;

		for(size_t i = target->getArgumentCount() - 1; i < params.size(); i++)
		{
			auto r = params[i]->codegen(cgi).result;
			fir::Value* val = r.first;
			fir::Value* valP = r.second;

			if(cgi->isAnyType(variadicType))
			{
				variadics.push_back(cgi->makeAnyFromValue(val, valP).result.first);
			}
			else if(variadicType != val->getType())
			{
				variadics.push_back(cgi->autoCastType(variadicType, val, valP));
			}
			else
			{
				variadics.push_back(val);
			}
		}

		// make the array thing.
		fir::Type* arrtype = fir::ArrayType::get(variadicType, variadics.size());
		fir::Value* rawArrayPtr = cgi->allocateInstanceInBlock(arrtype);

		for(size_t i = 0; i < variadics.size(); i++)
		{
			auto gep = cgi->builder.CreateConstGEP2(rawArrayPtr, 0, i);
			cgi->builder.CreateStore(variadics[i], gep);
		}

		fir::Value* arrPtr = cgi->builder.CreateConstGEP2(rawArrayPtr, 0, 0);
		fir::Value* llar = cgi->createLLVariableArray(arrPtr, fir::ConstantInt::getInt64(variadics.size())).result.first;
		args.push_back(llar);
	}



	// might not be a good thing to always do.
	// TODO: check this.
	// makes sure we call the function in our own module, because llvm only allows that.

	target = cgi->module->getFunction(target->getName());
	iceAssert(target);

	return Result_t(cgi->builder.CreateCall(target, args), 0);
}
















