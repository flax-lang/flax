// FuncCallCodegen.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "ast.h"
#include "codegen.h"
#include "runtimefuncs.h"

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


static fir::Function* instantiateGenericFunctionAsParameter(CodegenInstance* cgi, FuncCall* fc, fir::Value* val, fir::FunctionType* ft,
	fir::FunctionType* oldft, Expr* param)
{
	iceAssert(oldft);
	if(oldft->toFunctionType()->isGenericFunction())
	{
		iceAssert(!ft->isGenericFunction());

		fir::Function* oldf = dynamic_cast<fir::Function*>(val);

		// oldf can be null
		fir::Function* res = cgi->instantiateGenericFunctionUsingValueAndType(param, oldf, oldft, ft, dynamic_cast<MemberAccess*>(param));
		iceAssert(res);

		// rewrite history
		return res;
	}

	return 0;
}

static std::vector<fir::Value*> _checkAndCodegenFunctionCallParameters(CodegenInstance* cgi, FuncCall* fc, fir::FunctionType* ft,
	std::vector<Expr*> params, bool variadic, bool cvar)
{
	std::vector<fir::Value*> args;
	std::vector<fir::Value*> argptrs;

	if(!variadic)
	{
		size_t cur = 0;
		for(Expr* e : params)
		{
			bool checkcv = cvar && cur >= ft->getArgumentTypes().size() - 1;

			fir::Value* arg = 0;
			fir::Value* argptr = 0;
			std::tie(arg, argptr) = e->codegen(cgi);

			if(!checkcv && arg->getType()->isFunctionType() && ft->getArgumentTypes()[cur]->isFunctionType())
			{
				fir::Function* res = instantiateGenericFunctionAsParameter(cgi, fc, arg, ft->getArgumentTypes()[cur]->toFunctionType(),
					arg->getType()->toFunctionType(), e);

				if(res) arg = res;
			}


			if(arg == nullptr || arg->getType()->isVoidType())
				GenError::nullValue(cgi, e);

			if(checkcv && (arg->getType()->isStructType() || arg->getType()->isClassType() || arg->getType()->isTupleType()))
			{
				error(e, "Compound values (structs, classes and tuples) cannot be passed to C-style variadic functions");
			}
			else if(checkcv && arg->getType()->isArrayType())
			{
				error(e, "Arrays cannot be passed to C-style variadic functions. Cast to a pointer type instead.");
			}
			else if(checkcv && arg->getType()->isStringType())
			{
				// this function knows what to do.
				arg = cgi->autoCastType(fir::Type::getInt8Ptr(cgi->getContext()), arg, argptr);
			}
			else if(checkcv)
			{
				// check if we need to promote the argument type
				if(arg->getType() == fir::Type::getFloat32())
					arg = cgi->irb.CreateFExtend(arg, fir::Type::getFloat64());

				// don't need to worry about signedness for this; if you're smaller than int32,
				// int32 can represent you even if you're unsigned
				else if(arg->getType()->isIntegerType() && arg->getType()->toPrimitiveType()->getIntegerBitWidth() < 32)
					arg = cgi->irb.CreateIntSizeCast(arg, arg->getType()->isSignedIntType() ? fir::Type::getInt32() : fir::Type::getUint32());
			}

			args.push_back(arg);
			argptrs.push_back(argptr);

			cur++;
		}

		for(size_t i = 0; i < std::min(args.size(), ft->getArgumentTypes().size()); i++)
		{
			if(ft->getArgumentN(i) != args[i]->getType())
				args[i] = cgi->autoCastType(ft->getArgumentN(i), args[i], argptrs[i]);

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

			fir::Value* arg = 0;
			fir::Value* argp = 0;
			std::tie(arg, argp) = ex->codegen(cgi);


			if(arg->getType()->isFunctionType() && ft->getArgumentTypes()[i]->isFunctionType())
			{
				fir::Function* res = instantiateGenericFunctionAsParameter(cgi, fc, arg, ft->getArgumentTypes()[i]->toFunctionType(),
					arg->getType()->toFunctionType(), ex);

				if(res) arg = res;
			}










			if(arg == nullptr || arg->getType()->isVoidType())
				GenError::nullValue(cgi, ex);

			if(ft->getArgumentN(i) != arg->getType())
				arg = cgi->autoCastType(ft->getArgumentN(i), arg, argp);

			if(ft->getArgumentN(i) != arg->getType())
			{
				error(fc, "Argument %zu of function call is mismatched; expected '%s', got '%s'", i + 1,
					ft->getArgumentN(i)->str().c_str(), arg->getType()->str().c_str());
			}

			args.push_back(arg);
			argptrs.push_back(argp);
		}


		// special case: we can directly forward the arguments
		if(params.back()->getType(cgi)->isParameterPackType() && params.back()->getType(cgi)->toParameterPackType()->getElementType()
			== ft->getArgumentTypes().back()->toParameterPackType()->getElementType())
		{
			args.push_back(params.back()->codegen(cgi).value);
		}
		else
		{
			// do the last.
			fir::Type* variadicType = ft->getArgumentTypes().back()->toParameterPackType()->getElementType();
			std::vector<fir::Value*> variadics;

			for(size_t i = ft->getArgumentTypes().size() - 1; i < params.size(); i++)
			{
				fir::Value* val = 0;
				fir::Value* valp = 0;
				std::tie(val, valp) = params[i]->codegen(cgi);

				if(cgi->isAnyType(variadicType))
				{
					variadics.push_back(cgi->makeAnyFromValue(val, valp).value);
				}
				else if(variadicType != val->getType())
				{
					variadics.push_back(cgi->autoCastType(variadicType, val, valp));
				}
				else
				{
					variadics.push_back(val);
				}
			}

			fir::Value* pack = cgi->createParameterPack(variadicType, variadics).value;
			args.push_back(pack);
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
	else if(fir::PrimitiveType::fromBuiltin(this->name))
	{
		fir::Type* type = fir::PrimitiveType::fromBuiltin(this->name);

		// get our parameters.
		// strings can take more than one
		if(this->params.size() != 1 && !type->isStringType())
			error(this, "Builtin type initialisers must be called with exactly one argument (have %zu)", this->params.size());

		auto res = this->params[0]->codegen(cgi);
		fir::Value* arg = res.value;
		iceAssert(arg);


		// special case for string
		if(type->isStringType())
		{
			// make a string with this.
			if(dynamic_cast<StringLiteral*>(this->params[0]) && this->params.size() == 1)
			{
				// lol wtf
				return cgi->makeStringLiteral(dynamic_cast<StringLiteral*>(this->params[0])->str);
			}
			else if(arg->getType()->isStringType() && this->params.size() == 1)
			{
				return Result_t(arg, 0);
			}


			if(arg->getType() == fir::Type::getInt8Ptr())
			{
				// first get the length
				fir::Function* strlenf = cgi->getOrDeclareLibCFunc("strlen");
				fir::Function* mallocf = cgi->getOrDeclareLibCFunc(ALLOCATE_MEMORY_FUNC);
				fir::Function* memcpyf = cgi->module->getIntrinsicFunction("memmove");

				fir::Value* len = 0;
				if(this->params.size() == 1)
				{
					len = cgi->irb.CreateCall1(strlenf, arg);
				}
				else if(this->params.size() == 2)
				{
					fir::Value* l = this->params[1]->codegen(cgi).value;
					if(!l->getType()->isIntegerType())
					{
						error(this, "Expected integer type in second (length) parameter of string initialiser; have '%s'",
							l->getType()->str().c_str());
					}

					if(l->getType() != fir::Type::getInt64())
					{
						l = cgi->irb.CreateIntSignednessCast(l, l->getType()->toPrimitiveType()->getOppositeSignedType());
						if(l->getType() != fir::Type::getInt64())
							l = cgi->irb.CreateIntSizeCast(l, fir::Type::getInt64());
					}


					// make l the length
					len = l;
				}
				else
				{
					error(this, "Invalid number of parameters for string initialiser (have %zu, max is 2)", this->params.size());
				}






				auto i64size = 8;/*cgi->execTarget->getTypeSizeInBytes(fir::Type::getInt64());*/
				fir::Value* alloclen = cgi->irb.CreateAdd(len, fir::ConstantInt::getInt64(1 + i64size));

				fir::Value* buf = cgi->irb.CreateCall1(mallocf, alloclen);

				// add 8 bytes to this, skip the refcount
				buf = cgi->irb.CreatePointerAdd(buf, fir::ConstantInt::getInt64(i64size));

				// memcpy.
				cgi->irb.CreateCall(memcpyf, { buf, arg, len, fir::ConstantInt::getInt32(0), fir::ConstantInt::getBool(0) });

				// null terminator
				cgi->irb.CreateStore(fir::ConstantInt::getInt8(0), cgi->irb.CreatePointerAdd(buf, len));

				// ok, store everything.
				fir::Value* str = cgi->irb.CreateValue(fir::Type::getStringType());

				str = cgi->irb.CreateSetStringData(str, buf);
				str = cgi->irb.CreateSetStringLength(str, len);
				cgi->irb.CreateSetStringRefCount(str, fir::ConstantInt::getInt64(1));

				auto aa = cgi->irb.CreateImmutStackAlloc(str->getType(), str);
				cgi->addRefCountedValue(aa);
				return Result_t(str, aa);
			}
		}
		else if(type->isCharType())
		{
			if(arg->getType()->isStringType())
			{
				// needs to be constant
				if(auto cs = dynamic_cast<fir::ConstantString*>(arg))
				{
					std::string s = cs->getValue();
					if(s.length() == 0)
						error(this, "Empty character literals are not allowed");

					else if(s.length() > 1)
						error("Character literals must, obviously, contain only one (ASCII) character");

					char c = s[0];
					return Result_t(fir::ConstantChar::get(c), 0);
				}
				else
				{
					error(this, "Character literals need to be constant");
				}
			}
			else if(arg->getType()->isIntegerType())
			{
				// truncate if required
				if(arg->getType() != fir::Type::getInt8() && arg->getType() != fir::Type::getUint8())
					error(this, "Invalid instantiation of char from non-i8 type integer (have '%s')", arg->getType()->str().c_str());

				if(arg->getType() == fir::Type::getUint8())
					arg = cgi->irb.CreateIntSignednessCast(arg, fir::Type::getInt8());

				return Result_t(cgi->irb.CreateBitcast(arg, fir::Type::getCharType()), 0);
			}
		}
		else
		{
			if(arg->getType() != type)
				arg = cgi->autoCastType(type, arg, res.pointer);

			if(arg->getType() != type)
			{
				error(this, "Invalid argument type '%s' to builtin type initialiser for type '%s'", arg->getType()->str().c_str(),
					type->str().c_str());
			}

			// ok, just return the thing.
			return Result_t(arg, 0);
		}
	}
	else if(fir::Value* fv = cgi->getSymInst(this, this->name))
	{
		this->cachedResolveTarget.resolved = true;

		if(!fv->getType()->getPointerElementType()->isFunctionType())
		{
			error(this, "'%s' (of type '%s') is not a function, and cannot be called", this->name.c_str(),
				fv->getType()->getPointerElementType()->str().c_str());
		}

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

		std::map<Func*, std::pair<std::string, Expr*>> errs;
		if(!rt.resolved)
		{
			auto pair = cgi->tryResolveGenericFunctionCall(this, &errs);
			if(pair.firFunc || pair.funcDef)
				rt = Resolved_t(pair);

			else
				rt = Resolved_t();
		}


		if(!rt.resolved)
		{
			// label
			failedToFind:

			GenError::prettyNoSuchFunctionError(cgi, this, this->name, this->params, errs);
		}


		if(rt.t.firFunc == 0)
		{
			// generate it.
			iceAssert(rt.t.funcDecl);

			// printf("expediting function call to %s\n", this->name.c_str());
			rt.t.funcDecl->codegen(cgi);

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
		else if(fir::PrimitiveType::fromBuiltin(this->name))
		{
			fir::Type* type = fir::PrimitiveType::fromBuiltin(this->name);
			return type;
		}
		else if(fir::Value* fv = cgi->getSymInst(this, this->name))
		{
			if(!fv->getType()->getPointerElementType()->isFunctionType())
				error(this, "'%s' is not a function, and cannot be called", this->name.c_str());

			fir::FunctionType* ft = fv->getType()->getPointerElementType()->toFunctionType();
			iceAssert(ft);

			return ft->getReturnType();
		}
		else
		{
			std::map<Func*, std::pair<std::string, Expr*>> errs;
			auto genericMaybe = cgi->tryResolveGenericFunctionCall(this, &errs);
			if(genericMaybe.firFunc)
			{
				this->cachedResolveTarget = Resolved_t(genericMaybe);
				return genericMaybe.firFunc->getReturnType();
			}

			GenError::prettyNoSuchFunctionError(cgi, this, this->name, this->params, errs);
		}
	}

	return rt.t.funcDecl->getType(cgi);
}












