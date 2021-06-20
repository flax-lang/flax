// call.cpp
// Copyright (c) 2014 - 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include <set>

#include "sst.h"
#include "memorypool.h"
#include "codegen.h"
#include "gluecode.h"

util::hash_map<std::string, size_t> cgn::CodegenState::getNameIndexMap(sst::FunctionDefn* fd)
{
	util::hash_map<std::string, size_t> idxmap;

	for(size_t i = 0; i < fd->params.size(); i++)
		idxmap[fd->params[i].name] = i;

	return idxmap;
}






static std::vector<fir::Value*> _codegenAndArrangeFunctionCallArguments(cgn::CodegenState* cs, fir::FunctionType* ft,
	const std::vector<FnCallArgument>& arguments, const util::hash_map<std::string, size_t>& idxmap,
	const util::hash_map<size_t, sst::Expr*>& defaultArgumentValues)
{
	bool fvararg = ft->isVariadicFunc();
	size_t numNormalArgs = ft->getArgumentCount() + (fvararg ? -1 : 0);

	util::hash_map<size_t, sst::Expr*> argExprs;
	util::hash_map<sst::Expr*, size_t> revArgExprs;

	// this thing below operates similarly to the list solver in typecheck/polymorph/solver.cpp
	size_t last_arg = std::min(numNormalArgs, arguments.size());

	size_t positionalCounter = 0;
	size_t varArgStart = numNormalArgs;
	for(size_t i = 0; i < last_arg; i++)
	{
		const auto& arg = arguments[i];

		if(!arg.name.empty())
		{
			auto it = idxmap.find(arg.name);
			iceAssert(it != idxmap.end());

			argExprs[it->second] = arg.value;
			revArgExprs[arg.value] = it->second;

			if(defaultArgumentValues.find(it->second) == defaultArgumentValues.end())
				positionalCounter++;
		}
		else
		{
			// so, `positionalCounter` counts the paramters on the declaration-side. thus, once we encounter a default value,
			// it must mean that the rest of the parameters will be optional as well.
			//* ie. we've passed all the positional arguments already, leaving the optional ones, which means every argument from
			//* here onwards (including this one) must be named. since this is *not* named, we just skip straight to the varargs if
			//* it was present.
			if(fvararg && defaultArgumentValues.find(positionalCounter) != defaultArgumentValues.end())
			{
				varArgStart = i;
				break;
			}

			argExprs[positionalCounter] = arg.value;
			revArgExprs[arg.value] = positionalCounter;

			positionalCounter++;
		}
	}

	for(size_t i = 0; i < numNormalArgs; i++)
	{
		if(argExprs.find(i) == argExprs.end())
		{
			auto it = defaultArgumentValues.find(i);
			if(it == defaultArgumentValues.end())
				error(cs->loc(), "missing value for argument %d", i);

			argExprs[i] = it->second;
			revArgExprs[it->second] = i;
		}
	}

	auto doCastIfNecessary = [cs](const Location& loc, fir::Value* val, fir::Type* infer) -> fir::Value* {

		if(!infer)
			return val;

		if(val->getType() != infer)
		{
			val = cs->oneWayAutocast(val, infer);

			if(val->getType() != infer)
			{
				auto errs = SpanError::make(SimpleError::make(loc, "mismatched type in function call; parameter has type '%s', "
					"but given argument has type '%s'", infer, val->getType()));

				errs->postAndQuit();
			}
		}

		return val;
	};





	std::vector<fir::Value*> values(argExprs.size());
	{
		for(size_t i = 0; i < argExprs.size(); i++)
		{
			// this extra complexity is to ensure we codegen arguments from left-to-right!
			auto arg = argExprs[i];
			auto k = revArgExprs[arg];

			auto infer = ft->getArgumentN(k);
			auto val = arg->codegen(cs, infer).value;

			//! RAII: COPY CONSTRUCTOR CALL
			//? the copy constructor is called when passed as an argument to a function call
			//* copyRAIIValue will just return 'val' if it is not a class type, so we don't check it here!
			val = cs->copyRAIIValue(val);

			val = doCastIfNecessary(arg->loc, val, infer);
			values[k] = val;
		}
	}


	// check the variadic arguments. note that IRBuilder will handle actually wrapping the values up into a slice
	// and/or creating an empty slice and/or forwarding an existing slice. we just need to supply the values.
	for(size_t i = varArgStart; i < arguments.size(); i++)
	{
		auto arg = arguments[i].value;
		fir::Type* infer = 0;

		if(fvararg)
		{
			auto vararrty = ft->getArgumentN(ft->getArgumentCount() - 1);

			// if forwarding perfectly, then infer as the slice type, instead of the element type.
			if(i == arguments.size() - 1 && arg->type->isVariadicArrayType())
			{
				// perfect forwarding.
				infer = vararrty;
			}
			else
			{
				iceAssert(vararrty->isVariadicArrayType());
				infer = vararrty->getArrayElementType();
			}
		}

		auto val = arg->codegen(cs, infer).value;
		val = doCastIfNecessary(arg->loc, val, infer);


		if(ft->isCStyleVarArg())
		{
			// auto-convert strings and char slices into char* when passing to va_args
			if(val->getType()->isCharSliceType())
				val = cs->irb.GetArraySliceData(val);

			// also, see if we need to promote the type!
			// anything < int gets promoted to int; float -> double
			else if(val->getType() == fir::Type::getFloat32())
				val = cs->irb.FExtend(val, fir::Type::getFloat64());

			// don't need to worry about signedness for this; if you're smaller than int32,
			// int32 can represent you even if you're unsigned
			else if(val->getType()->isIntegerType() && val->getType()->toPrimitiveType()->getIntegerBitWidth() < 32)
				val = cs->irb.IntSizeCast(val, val->getType()->isSignedIntType() ? fir::Type::getInt32() : fir::Type::getUint32());

			else if(val->getType()->isBoolType())
				val = cs->irb.IntZeroExt(val, fir::Type::getInt32());
		}

		values.push_back(val);
	}

	return values;
}


std::vector<fir::Value*> cgn::CodegenState::codegenAndArrangeFunctionCallArguments(sst::Defn* target, fir::FunctionType* ft,
	const std::vector<FnCallArgument>& arguments)
{
	util::hash_map<std::string, size_t> idxmap;
	util::hash_map<size_t, sst::Expr*> defaultArgs;

	if(auto fd = dcast(sst::FunctionDefn, target))
	{
		idxmap = this->getNameIndexMap(fd);

		zfu::foreachIdx(fd->params, [&defaultArgs](const FnParam& arg, size_t idx) {
			if(arg.defaultVal)
				defaultArgs[idx] = arg.defaultVal;
		});
	}

	return _codegenAndArrangeFunctionCallArguments(this, ft, arguments, idxmap, defaultArgs);
}

CGResult sst::FunctionCall::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	if(!this->target)
		error(this, "failed to find target for function call to '%s'", this->name);

	// check this target
	fir::Value* vf = 0;
	fir::FunctionType* ft = 0;


	if(dcast(VarDefn, this->target))
	{
		// ok, we're calling a variable.
		// the below stuff ain't gonna work without some intervention

		CGResult defn;
		CGResult r = cs->valueMap[this->target];

		if(r.value)
		{
			defn = r;
		}
		else if(cs->isInMethodBody())
		{
			defn = cs->getStructFieldImplicitly(this->name);
		}
		else
		{
			error(this, "no such '%s'", this->name);
		}

		iceAssert(defn.value);
		vf = defn.value;
	}
	else if(auto fd = dcast(FunctionDefn, this->target); fd && fd->isVirtual)
	{
		// ok then.
		auto ret = cs->callVirtualMethod(this);
		cs->addRAIIOrRCValueIfNecessary(ret);

		return CGResult(ret);
	}
	else
	{
		vf = this->target->codegen(cs).value;
	}


	if(vf->getType()->isFunctionType())
	{
		ft = vf->getType()->toFunctionType();
	}
	else
	{
		// we should have disallowed this already in the typechecker.
		// TODO: is the usecase then to just cast to a function type?
		// eg. let entry = (0x400000) as (fn()->int) or something
		iceAssert(false && "somehow you got a pointer-to-function?!");

		// auto vt = vf->getType();
		// iceAssert(vt->isPointerType() && vt->getPointerElementType()->isFunctionType());

		// ft = vt->getPointerElementType()->toFunctionType();

		// warn(this, "prefer using functions to function pointers");
	}

	iceAssert(ft);

	//! SELF HANDLING (INSERTION) (CODEGEN)
	if(auto fd = dcast(FunctionDefn, this->target); fd && fd->parentTypeForMethod && cs->isInMethodBody() && this->isImplicitMethodCall)
	{
		auto fake = util::pool<RawValueExpr>(this->loc, fd->parentTypeForMethod->getPointerTo());
		fake->rawValue = CGResult(cs->irb.AddressOf(cs->getMethodSelf(), true));

		this->arguments.insert(this->arguments.begin(), FnCallArgument(this->loc, "this", fake, 0));
	}

	size_t numArgs = ft->getArgumentCount();
	if(ft->isCStyleVarArg() && this->arguments.size() < numArgs)
	{
		error(this, "need at least %d arguments to call variadic function '%s', only have %d",
			numArgs, this->name, this->arguments.size());
	}


	auto args = cs->codegenAndArrangeFunctionCallArguments(this->target, ft, this->arguments);

	fir::Value* ret = 0;

	if(fir::Function* func = dcast(fir::Function, vf))
	{
		ret = cs->irb.Call(func, args);
	}
	else if(vf->getType()->isFunctionType())
	{
		ret = cs->irb.CallToFunctionPointer(vf, ft, args);
	}
	else
	{
		iceAssert(vf->getType()->getPointerElementType()->isFunctionType());
		auto fptr = cs->irb.ReadPtr(vf);

		ret = cs->irb.CallToFunctionPointer(fptr, ft, args);
	}

	// do the refcounting if we need to
	cs->addRAIIOrRCValueIfNecessary(ret);
	return CGResult(ret);
}



static CGResult callBuiltinTypeConstructor(cgn::CodegenState* cs, fir::Type* type, const std::vector<sst::Expr*>& args)
{
	// for non-strings it's trivial
	if(args.empty())
	{
		return CGResult(cs->getDefaultValue(type));
	}
	else
	{
		iceAssert(args.size() == 1);
		auto ret = cs->oneWayAutocast(args[0]->codegen(cs, type).value, type);

		if(type != ret->getType())
			error(args[0], "mismatched type in builtin type initialiser; expected '%s', found '%s'", type, ret->getType());

		return CGResult(ret);
	}
}




CGResult sst::ExprCall::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	if(auto te = dcast(sst::TypeExpr, this->callee))
		return callBuiltinTypeConstructor(cs, te->type, this->arguments);

	iceAssert(this->callee);

	fir::Value* fn = this->callee->codegen(cs).value;
	iceAssert(fn->getType()->isFunctionType());

	auto ft = fn->getType()->toFunctionType();

	if(ft->getArgumentCount() != this->arguments.size())
	{
		if((!ft->isVariadicFunc() && !ft->isCStyleVarArg()) || this->arguments.size() < ft->getArgumentCount())
		{
			error(this, "mismatched number of arguments; expected %d, but %d were given",
				ft->getArgumentCount(), this->arguments.size());
		}
	}

	std::vector<FnCallArgument> fcas = zfu::map(this->arguments, [](sst::Expr* arg) -> FnCallArgument {
		return FnCallArgument(arg->loc, "", arg, /* orig: */ nullptr);
	});

	std::vector<fir::Value*> args = cs->codegenAndArrangeFunctionCallArguments(/* targetDefn: */ nullptr, ft, fcas);

	auto ret = cs->irb.CallToFunctionPointer(fn, ft, args);

	cs->addRAIIOrRCValueIfNecessary(ret);
	return CGResult(ret);
}
















