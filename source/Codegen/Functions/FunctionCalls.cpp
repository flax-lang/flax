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
	fir::Value* ai = this->getStackAlloc(tp->first, "tmp");

	args.insert(args.begin(), ai);

	fir::Function* initfunc = this->getStructInitialiser(user, tp, args);

	this->builder.CreateCall(initfunc, args);
	fir::Value* val = this->builder.CreateLoad(ai);

	return Result_t(val, ai);
}

Result_t FuncCall::codegen(CodegenInstance* cgi, fir::Value* extra)
{
	// always try the type first.
	if(TypePair_t* tp = cgi->getTypeByString(this->name))
	{
		std::vector<fir::Value*> args;
		for(Expr* e : this->params)
			args.push_back(e->codegen(cgi).result.first);

		return cgi->callTypeInitialiser(tp, this, args);
	}

	std::vector<fir::Value*> args;

	fir::Function* target = 0;



	// we're not a generic function.
	if(!this->cachedResolveTarget.resolved)
	{
		Resolved_t rt = cgi->resolveFunction(this, this->name, this->params);

		if(!rt.resolved)
		{
			auto pair = cgi->tryResolveGenericFunctionCall(this);
			if(pair.first || pair.second)
				rt = Resolved_t(pair);

			else
				rt = Resolved_t();
		}


		if(!rt.resolved)
		{
			// label
			failedToFind:

			GenError::prettyNoSuchFunctionError(cgi, this, this->name, this->params);
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
		std::vector<fir::Value*> argPtrs;

		for(Expr* e : this->params)
		{
			ValPtr_t res = e->codegen(cgi).result;
			fir::Value* arg = res.first;

			if(arg == nullptr || arg->getType()->isVoidType())
				GenError::nullValue(cgi, e);

			if(checkCVarArg && (arg->getType()->isStructType() || arg->getType()->isClassType() || arg->getType()->isTupleType()))
			{
				fir::Type* st = arg->getType();
				if(st->isClassType() && st->toClassType()->getClassName().str() == "String")
				{
					// this function knows what to do.
					arg = cgi->autoCastType(fir::PointerType::getInt8Ptr(cgi->getContext()), arg, res.second);
				}
				else if(st->isClassType() || st->isStructType())
				{
					warn(e, "Passing structs to C-style variadic functions can have unexpected results.");
				}
			}

			args.push_back(arg);
			argPtrs.push_back(res.second);
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
				GenError::nullValue(cgi, ex);

			args.push_back(arg);
		}


		// special case: we can directly forward the arguments
		if(cgi->getExprType(params.back())->isLLVariableArrayType()
			&& cgi->getExprType(params.back())->toLLVariableArray()->getElementType() == target->getArguments().back()->getType()->toLLVariableArray()->getElementType())
		{
			args.push_back(params.back()->codegen(cgi).result.first);
		}
		else
		{
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
			fir::Value* rawArrayPtr = cgi->getStackAlloc(arrtype);

			for(size_t i = 0; i < variadics.size(); i++)
			{
				auto gep = cgi->builder.CreateConstGEP2(rawArrayPtr, 0, i);
				cgi->builder.CreateStore(variadics[i], gep);
			}

			fir::Value* arrPtr = cgi->builder.CreateConstGEP2(rawArrayPtr, 0, 0);
			fir::Value* llar = cgi->createLLVariableArray(arrPtr, fir::ConstantInt::getInt64(variadics.size())).result.first;
			args.push_back(llar);
		}
	}



	// might not be a good thing to always do.
	// TODO: check this.
	// makes sure we call the function in our own module, because llvm only allows that.

	auto thistarget = cgi->module->getOrCreateFunction(target->getName(), target->getType(), target->linkageType);
	return Result_t(cgi->builder.CreateCall(thistarget, args), 0);
}
















