// call.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "sst.h"
#include "codegen.h"

std::unordered_map<std::string, size_t> cgn::CodegenState::getNameIndexMap(sst::FunctionDefn* fd)
{
	std::unordered_map<std::string, size_t> idxmap;

	for(size_t i = 0; i < fd->params.size(); i++)
		idxmap[fd->params[i].name] = i;

	return idxmap;
}



std::vector<fir::Value*> cgn::CodegenState::codegenAndArrangeFunctionCallArguments(sst::Defn* target, fir::FunctionType* ft,
	const std::vector<FnCallArgument>& arguments)
{
	std::unordered_map<std::string, size_t> idxmap;
	if(auto fd = dcast(sst::FunctionDefn, target))
		idxmap = this->getNameIndexMap(fd);

	return this->codegenAndArrangeFunctionCallArguments(ft, arguments, idxmap);
}




std::vector<fir::Value*> cgn::CodegenState::codegenAndArrangeFunctionCallArguments(fir::FunctionType* ft,
	const std::vector<FnCallArgument>& arguments, const std::unordered_map<std::string, size_t>& idxmap)
{
	// do this so we can index directly.
	auto numArgs = ft->getArgumentTypes().size();
	auto ret = std::vector<fir::Value*>(arguments.size());

	size_t counter = 0;
	for(auto arg : arguments)
	{
		size_t i = (arg.name == "" ? counter : idxmap.find(arg.name)->second);

		fir::Type* inf = 0;
		if(i < numArgs)
			inf = ft->getArgumentN(i);

		auto vr = arg.value->codegen(this, inf);
		auto val = vr.value;

		if(val->getType()->isConstantNumberType())
		{
			auto cv = dcast(fir::ConstantValue, val);
			iceAssert(cv);

			val = this->unwrapConstantNumber(cv);
		}

		if(i < numArgs)
		{
			if(val->getType() != ft->getArgumentN(i))
			{
				vr = this->oneWayAutocast(vr, ft->getArgumentN(i));
				val = vr.value;
			}

			// still?
			if(val->getType() != ft->getArgumentN(i))
			{
				error(arg.loc, "Mismatched type in function call; parameter has type '%s', but given argument has type '%s'",
					ft->getArgumentN(i), val->getType());
			}
		}
		else if(val->getType()->isStringType())
		{
			// auto-convert strings into char* when passing to va_args
			val = this->irb.GetStringData(val);
		}


		// args.push_back(val);
		ret[i] = val;

		counter++;
	}

	return ret;
}


CGResult sst::FunctionCall::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	if(!this->target)
		error(this, "Failed to find target for function call to '%s'", this->name);

	// check this target
	fir::Value* vf = 0;
	fir::FunctionType* ft = 0;

	if(auto vd = dcast(VarDefn, this->target))
	{
		// ok, we're calling a variable.
		// the below stuff ain't gonna work without some intervention

		CGResult defn;
		CGResult r = cs->valueMap[this->target];
		// auto r = cs->findValueInTree(this->name);

		if(r.value || r.pointer)
		{
			if(!r.value)
				defn = CGResult(cs->irb.Load(r.pointer), r.pointer);

			else
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

		warn(this, "Prefer using functions to function pointers");
	}

	iceAssert(ft);

	if(auto fd = dcast(FunctionDefn, this->target); fd && fd->parentTypeForMethod && cs->isInMethodBody() && this->isImplicitMethodCall)
	{
		auto fake = new RawValueExpr(this->loc, fd->parentTypeForMethod->getPointerTo());
		fake->rawValue = CGResult(cs->getMethodSelf());

		this->arguments.insert(this->arguments.begin(), FnCallArgument(this->loc, "self", fake));
	}

	size_t numArgs = ft->getArgumentTypes().size();
	if(!ft->isCStyleVarArg() && this->arguments.size() != numArgs)
	{
		error(this, "Mismatch in number of arguments in call to '%s'; %zu %s provided, but %zu %s expected",
			this->name, this->arguments.size(), this->arguments.size() == 1 ? "was" : "were", numArgs,
			numArgs == 1 ? "was" : "were");
	}
	else if(ft->isCStyleVarArg() && this->arguments.size() < numArgs)
	{
		error(this, "Need at least %zu arguments to call variadic function '%s', only have %zu",
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
		auto fptr = cs->irb.Load(vf);

		ret = cs->irb.CallToFunctionPointer(fptr, ft, args);
	}

	// do the refcounting if we need to
	if(cs->isRefCountedType(ret->getType()))
		cs->addRefCountedValue(ret);

	return CGResult(ret);
}



CGResult sst::ExprCall::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	fir::Value* fn = this->callee->codegen(cs).value;
	iceAssert(fn->getType()->isFunctionType());

	auto ft = fn->getType()->toFunctionType();

	if(ft->getArgumentTypes().size() != this->arguments.size() && !ft->isVariadicFunc())
	{
		error(this, "Mismatched number of arguments; expected %zu, but %zu were given",
			ft->getArgumentTypes().size(), this->arguments.size());
	}

	std::vector<fir::Value*> args;
	for(size_t i = 0; i < this->arguments.size(); i++)
	{
		fir::Type* inf = 0;

		if(i < ft->getArgumentTypes().size())
			inf = ft->getArgumentN(i);

		else
			inf = ft->getArgumentTypes().back()->getArrayElementType();

		auto rarg = this->arguments[i]->codegen(cs, inf);
		auto arg = cs->oneWayAutocast(rarg, inf).value;

		if(!arg || arg->getType() != inf)
		{
			error(this->arguments[i], "Mismatched types in argument %zu; expected type '%s', but given type '%s'", inf,
				arg ? arg->getType() : fir::Type::getVoid());
		}

		args.push_back(arg);
	}

	auto ret = cs->irb.CallToFunctionPointer(fn, ft, args);
	return CGResult(ret);
}
















