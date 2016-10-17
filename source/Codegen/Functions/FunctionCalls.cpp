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

	this->irb.CreateCall(initfunc, args);
	fir::Value* val = this->irb.CreateLoad(ai);

	return Result_t(val, ai);
}



static std::deque<fir::Value*> _checkAndCodegenFunctionCallParameters(CodegenInstance* cgi, FuncCall* fc, fir::FunctionType* ft,
	std::deque<Expr*> params, bool variadic, bool cvar)
{
	std::deque<fir::Value*> args;

	if(!variadic)
	{
		std::vector<fir::Value*> argPtrs;

		size_t cur = 0;
		for(Expr* e : params)
		{
			bool checkcv = cvar && cur >= ft->getArgumentTypes().size() - 1;

			auto res = e->codegen(cgi);
			fir::Value* arg = res.value;

			if(arg == nullptr || arg->getType()->isVoidType())
				GenError::nullValue(cgi, e);

			if(checkcv && (arg->getType()->isStructType() || arg->getType()->isClassType() || arg->getType()->isTupleType()))
			{
				fir::Type* st = arg->getType();
				if(st->isClassType() || st->isStructType())
				{
					warn(e, "Passing structs to C-style variadic functions can have unexpected results.");
				}
			}
			else if(checkcv && arg->getType()->isStringType())
			{
				// this function knows what to do.
				arg = cgi->autoCastType(fir::Type::getInt8Ptr(cgi->getContext()), arg, res.pointer);
			}

			args.push_back(arg);
			argPtrs.push_back(res.pointer);

			cur++;
		}


		for(size_t i = 0; i < std::min(args.size(), ft->getArgumentTypes().size()); i++)
		{
			if(ft->getArgumentN(i) != args[i]->getType())
				args[i] = cgi->autoCastType(ft->getArgumentN(i), args[i], argPtrs[i]);

			if(ft->getArgumentN(i) != args[i]->getType())
			{
				error(fc, "Argument %zu of function call is mismatched; expected '%s', got '%s'", i + 1,
					ft->getArgumentN(i)->str().c_str(), args[i]->getType()->str().c_str());
			}
		}
	}
	else
	{
		// variadic.
		// remember, last argument is the llarray.
		// do until the penultimate argument.
		for(size_t i = 0; i < ft->getArgumentTypes().size() - 1; i++)
		{
			Expr* ex = params[i];

			fir::Value* arg = ex->codegen(cgi).value;

			if(arg == nullptr || arg->getType()->isVoidType())
				GenError::nullValue(cgi, ex);

			args.push_back(arg);
		}


		// special case: we can directly forward the arguments
		if(params.back()->getType(cgi)->isLLVariableArrayType() && params.back()->getType(cgi)->toLLVariableArrayType()->getElementType()
			== ft->getArgumentTypes().back()->toLLVariableArrayType()->getElementType())
		{
			args.push_back(params.back()->codegen(cgi).value);
		}
		else
		{
			// do the last.
			fir::Type* variadicType = ft->getArgumentTypes().back()->toLLVariableArrayType()->getElementType();
			std::deque<fir::Value*> variadics;

			for(size_t i = ft->getArgumentTypes().size() - 1; i < params.size(); i++)
			{
				auto r = params[i]->codegen(cgi);
				fir::Value* val = r.value;
				fir::Value* valP = r.pointer;

				if(cgi->isAnyType(variadicType))
				{
					variadics.push_back(cgi->makeAnyFromValue(val, valP).value);
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
				auto gep = cgi->irb.CreateConstGEP2(rawArrayPtr, 0, i);
				cgi->irb.CreateStore(variadics[i], gep);
			}

			fir::Value* arrPtr = cgi->irb.CreateConstGEP2(rawArrayPtr, 0, 0);
			fir::Value* llar = cgi->createLLVariableArray(arrPtr, fir::ConstantInt::getInt64(variadics.size())).value;
			args.push_back(llar);
		}
	}

	return args;
}
























static inline fir::Value* handleRefcountedThingIfNeeded(CodegenInstance* cgi, fir::Value* ret)
{
	if(cgi->isRefCountedType(ret->getType()))
	{
		fir::Value* tmp = cgi->irb.CreateImmutStackAlloc(ret->getType(), ret);
		cgi->addRefCountedValue(tmp);

		return tmp;
	}

	return 0;
}

Result_t FuncCall::codegen(CodegenInstance* cgi, fir::Value* extra)
{
	// always try the type first.
	if(TypePair_t* tp = cgi->getTypeByString(this->name))
	{
		std::vector<fir::Value*> args;
		for(Expr* e : this->params)
			args.push_back(e->codegen(cgi).value);

		return cgi->callTypeInitialiser(tp, this, args);
	}
	else if(fir::Value* fv = cgi->getSymInst(this, this->name))
	{
		this->cachedResolveTarget.resolved = true;

		if(!fv->getType()->getPointerElementType()->isFunctionType())
			error("'%s' is not a function, and cannot be called", this->name.c_str());

		fir::FunctionType* ft = fv->getType()->getPointerElementType()->toFunctionType();
		iceAssert(ft);


		auto args = _checkAndCodegenFunctionCallParameters(cgi, this, ft, this->params, ft->isVariadicFunc(), ft->isCStyleVarArg());

		fir::Value* fn = cgi->irb.CreateLoad(fv);
		fir::Value* ret = cgi->irb.CreateCallToFunctionPointer(fn, ft, args);

		return Result_t(ret, handleRefcountedThingIfNeeded(cgi, ret));
	}
	else if(extra && extra->getType()->isFunctionType())
	{
		this->cachedResolveTarget.resolved = true;

		fir::FunctionType* ft = extra->getType()->toFunctionType();
		iceAssert(ft);

		auto args = _checkAndCodegenFunctionCallParameters(cgi, this, ft, this->params, ft->isVariadicFunc(), ft->isCStyleVarArg());

		fir::Value* ret = cgi->irb.CreateCallToFunctionPointer(extra, ft, args);

		return Result_t(ret, handleRefcountedThingIfNeeded(cgi, ret));
	}



	fir::Function* target = 0;


	// we're not a generic function.
	if(!this->cachedResolveTarget.resolved)
	{
		Resolved_t rt = cgi->resolveFunction(this, this->name, this->params);

		if(!rt.resolved)
		{
			auto pair = cgi->tryResolveGenericFunctionCall(this);
			if(pair.firFunc || pair.funcDef)
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


		if(rt.t.firFunc == 0)
		{
			// generate it.
			iceAssert(rt.t.funcDecl);
			rt.t.funcDecl->codegen(cgi);

			// printf("expediting function call to %s\n", this->name.c_str());

			rt = cgi->resolveFunction(this, this->name, this->params);

			if(!rt.resolved) error("nani???");
			if(rt.t.firFunc == 0) goto failedToFind;
		}

		this->cachedResolveTarget = rt;
	}

	target = this->cachedResolveTarget.t.firFunc;




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


	auto args = _checkAndCodegenFunctionCallParameters(cgi, this, target->getType(), params, checkVariadic, checkCVarArg);


	// might not be a good thing to always do.
	// TODO: check this.
	// makes sure we call the function in our own module, because llvm only allows that.

	fir::Function* thistarget = cgi->module->getOrCreateFunction(target->getName(), target->getType(), target->linkageType);
	fir::Value* ret = cgi->irb.CreateCall(thistarget, args);


	return Result_t(ret, handleRefcountedThingIfNeeded(cgi, ret));
}



fir::Type* FuncCall::getType(CodegenInstance* cgi, bool allowFail, fir::Value* extra)
{
	Resolved_t rt = cgi->resolveFunction(this, this->name, this->params);
	if(!rt.resolved)
	{
		TypePair_t* tp = cgi->getTypeByString(this->name);
		if(tp)
		{
			return tp->first;
		}
		else
		{
			auto genericMaybe = cgi->tryResolveGenericFunctionCall(this);
			if(genericMaybe.firFunc)
			{
				this->cachedResolveTarget = Resolved_t(genericMaybe);
				return genericMaybe.firFunc->getReturnType();
			}
			// else if(genericMaybe.funcDef)
			// {
			// 	auto res = cgi->tryResolveGenericFunctionCallUsingCandidates(this, { genericMaybe.funcDef });
			// 	if(res.firFunc)
			// 	{
			// 		this->cachedResolveTarget = Resolved_t(res);
			// 		return res.firFunc->getReturnType();
			// 	}
			// }

			GenError::prettyNoSuchFunctionError(cgi, this, this->name, this->params);
		}
	}

	return rt.t.funcDecl->getType(cgi);
}












