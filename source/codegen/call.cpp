// call.cpp
// Copyright (c) 2014 - 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include "sst.h"
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
	const std::vector<FnCallArgument>& arguments, const util::hash_map<std::string, size_t>& idxmap)
{
	// do this so we can index directly.
	auto numArgs = ft->getArgumentTypes().size();
	auto ret = std::vector<fir::Value*>(arguments.size());

	size_t counter = 0;
	for(auto arg : arguments)
	{
		size_t i = (arg.name == "" ? counter : idxmap.find(arg.name)->second);

		fir::Type* inf = 0;
		if(ft->isVariadicFunc())
		{
			//? the reason for this discrepancy is that for va_arg functions, the '...' isn't really counted
			//? as an 'argument', whereas for flax-varargs we have a variadic slice thingy that is an actual argument.
			iceAssert(numArgs > 0);

			auto at = arg.value->type;

			if(i < numArgs - 1)
			{
				inf = ft->getArgumentN(i);
			}
			else if(i == numArgs - 1 && at->isArraySliceType() && at->getArrayElementType() == ft->getArgumentTypes().back()->getArrayElementType())
			{
				// perfect forwarding.
				inf = ft->getArgumentTypes().back();
			}
			else
			{
				iceAssert(ft->getArgumentTypes().back()->isVariadicArrayType());
				inf = ft->getArgumentTypes().back()->getArrayElementType();
			}
		}
		else if(i < numArgs)
		{
			inf = ft->getArgumentN(i);
		}



		auto vr = arg.value->codegen(cs, inf);
		auto val = vr.value;

		//* arguments are added to the refcounting list in the function,
		//* so we need to "pre-increment" the refcount here, so it does not
		//* get freed when the function returns.
		if(fir::isRefCountedType(val->getType()))
			cs->incrementRefCount(val);



		if(val->getType()->isConstantNumberType())
		{
			auto cv = dcast(fir::ConstantValue, val);
			iceAssert(cv);

			val = cs->unwrapConstantNumber(cv);
		}

		if(ft->isVariadicFunc() || i < numArgs)
		{
			// the inferred type should always be present, and should always be correct.
			iceAssert(inf);

			// cs syntax feels a little dirty.
			val = cs->oneWayAutocast(vr.value, inf);
			vr.value = val;


			if(val->getType() != inf)
			{
				auto errs = SpanError::make(SimpleError::make(arg.loc,
					"mismatched type in function call; parameter %d has type '%s', but given argument has type '%s'", i, inf, val->getType()));

				if(ft->isVariadicFunc() && i >= numArgs - 1)
				{
					errs->add(util::ESpan(arg.loc, strprintf("argument's type '%s' cannot be cast to the expected variadic element type '%s'",
						val->getType(), inf)));
				}

				errs->postAndQuit();
			}
		}
		else if(ft->isCStyleVarArg())
		{
			// auto-convert strings and char slices into char* when passing to va_args
			if(val->getType()->isStringType())
				val = cs->irb.GetSAAData(val);

			else if(val->getType()->isCharSliceType())
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
				val = cs->irb.IntSizeCast(val, fir::Type::getInt32());
		}

		ret[i] = val;
		counter++;
	}


	// if we're calling a variadic function without any extra args, insert the slice at the back.
	if(ft->isVariadicFunc() && arguments.size() == numArgs - 1)
	{
		auto et = ft->getArgumentTypes().back()->getArrayElementType();
		ret.push_back(fir::ConstantArraySlice::get(fir::ArraySliceType::getVariadic(et),
			fir::ConstantValue::getZeroValue(et->getPointerTo()), fir::ConstantInt::getNative(0)));
	}

	return ret;
}


std::vector<fir::Value*> cgn::CodegenState::codegenAndArrangeFunctionCallArguments(sst::Defn* target, fir::FunctionType* ft,
	const std::vector<FnCallArgument>& arguments)
{
	util::hash_map<std::string, size_t> idxmap;
	if(auto fd = dcast(sst::FunctionDefn, target))
		idxmap = this->getNameIndexMap(fd);

	return _codegenAndArrangeFunctionCallArguments(this, ft, arguments, idxmap);
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
		return CGResult(cs->callVirtualMethod(this));
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
		auto vt = vf->getType();
		iceAssert(vt->isPointerType() && vt->getPointerElementType()->isFunctionType());

		ft = vt->getPointerElementType()->toFunctionType();

		warn(this, "prefer using functions to function pointers");
	}

	iceAssert(ft);

	//! SELF HANDLING (INSERTION) (CODEGEN)
	if(auto fd = dcast(FunctionDefn, this->target); fd && fd->parentTypeForMethod && cs->isInMethodBody() && this->isImplicitMethodCall)
	{
		auto fake = new RawValueExpr(this->loc, fd->parentTypeForMethod->getPointerTo());
		fake->rawValue = CGResult(cs->irb.AddressOf(cs->getMethodSelf(), true));

		this->arguments.insert(this->arguments.begin(), FnCallArgument(this->loc, "self", fake, 0));
	}

	size_t numArgs = ft->getArgumentTypes().size();
	if(!ft->isCStyleVarArg() && !ft->isVariadicFunc() && this->arguments.size() != numArgs)
	{
		error(this, "mismatch in number of arguments in call to '%s'; %zu %s provided, but %zu %s expected",
			this->name, this->arguments.size(), this->arguments.size() == 1 ? "was" : "were", numArgs,
			numArgs == 1 ? "was" : "were");
	}
	else if((ft->isCStyleVarArg() || !ft->isVariadicFunc()) && this->arguments.size() < numArgs)
	{
		error(this, "need at least %zu arguments to call variadic function '%s', only have %zu",
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
	if(fir::isRefCountedType(ret->getType()))
		cs->addRefCountedValue(ret);

	return CGResult(ret);
}



static CGResult callBuiltinTypeConstructor(cgn::CodegenState* cs, fir::Type* type, const std::vector<sst::Expr*>& args)
{
	// for non-strings it's trivial
	if(args.empty())
	{
		return CGResult(cs->getDefaultValue(type));
	}
	else if(!type->isStringType())
	{
		iceAssert(args.size() == 1);
		auto ret = cs->oneWayAutocast(args[0]->codegen(cs, type).value, type);

		if(type != ret->getType())
			error(args[0], "mismatched type in builtin type initialiser; expected '%s', found '%s'", type, ret->getType());

		return CGResult(ret);
	}
	else
	{
		auto cloneTheSlice = [cs](fir::Value* slc) -> CGResult {

			iceAssert(slc->getType()->isCharSliceType());

			auto clonef = cgn::glue::string::getCloneFunction(cs);
			iceAssert(clonef);

			auto ret = cs->irb.Call(clonef, slc, fir::ConstantInt::getNative(0));
			cs->addRefCountedValue(ret);

			return CGResult(ret);
		};

		if(args.size() == 1)
		{
			iceAssert(args[0]->type->isCharSliceType());
			return cloneTheSlice(args[0]->codegen(cs, fir::Type::getCharSlice(false)).value);
		}
		else
		{
			iceAssert(args.size() == 2);
			iceAssert(args[0]->type == fir::Type::getInt8Ptr() || args[0]->type == fir::Type::getMutInt8Ptr());
			iceAssert(args[1]->type->isIntegerType());

			auto ptr = args[0]->codegen(cs).value;
			auto len = cs->oneWayAutocast(args[1]->codegen(cs, fir::Type::getNativeWord()).value, fir::Type::getNativeWord());

			auto slc = cs->irb.CreateValue(fir::Type::getCharSlice(false));
			slc = cs->irb.SetArraySliceData(slc, (ptr->getType()->isMutablePointer() ? cs->irb.PointerTypeCast(ptr, fir::Type::getInt8Ptr()) : ptr));
			slc = cs->irb.SetArraySliceLength(slc, len);

			return cloneTheSlice(slc);
		}
	}
}




CGResult sst::ExprCall::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	if(auto te = dcast(sst::TypeExpr, this->callee))
		return callBuiltinTypeConstructor(cs, te->type, this->arguments);


	fir::Value* fn = this->callee->codegen(cs).value;
	iceAssert(fn->getType()->isFunctionType());

	auto ft = fn->getType()->toFunctionType();

	if(ft->getArgumentTypes().size() != this->arguments.size())
	{
		if((!ft->isVariadicFunc() && !ft->isCStyleVarArg()) || this->arguments.size() < ft->getArgumentTypes().size())
		{
			error(this, "mismatched number of arguments; expected %zu, but %zu were given",
				ft->getArgumentTypes().size(), this->arguments.size());
		}
	}

	std::vector<FnCallArgument> fcas = util::map(this->arguments, [](sst::Expr* arg) -> FnCallArgument {
		return FnCallArgument(arg->loc, "", arg, /* orig: */ nullptr);
	});

	std::vector<fir::Value*> args = cs->codegenAndArrangeFunctionCallArguments(/* targetDefn: */ nullptr, ft, fcas);

	auto ret = cs->irb.CallToFunctionPointer(fn, ft, args);
	return CGResult(ret);
}
















