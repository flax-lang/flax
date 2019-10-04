// constructor.cpp
// Copyright (c) 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include "sst.h"
#include "codegen.h"
#include "memorypool.h"

fir::Value* cgn::CodegenState::getConstructedStructValue(fir::StructType* str, const std::vector<FnCallArgument>& args)
{
	fir::Value* value = this->irb.CreateValue(str);

	// set the arguments.
	if(args.size() > 0)
	{
		bool names = !args[0].name.empty();

		// i just keeps track of the index in case we're not using names.
		size_t i = 0;
		for(const auto& arg : args)
		{
			if(names)
			{
				iceAssert(str->hasElementWithName(arg.name));
				auto elmty = str->getElement(arg.name);

				value = this->irb.InsertValueByName(value, arg.name, arg.value->codegen(this, elmty).value);
			}
			else
			{
				iceAssert(str->getElementCount() > i);
				auto elmty = str->getElementN(i);

				value = this->irb.InsertValue(value, { i }, arg.value->codegen(this, elmty).value);
			}

			i++;
		}
	}

	if(fir::isRefCountedType(str))
		this->addRefCountedValue(value);

	return value;
}



fir::Value* cgn::CodegenState::constructClassWithArguments(fir::ClassType* cls, sst::FunctionDefn* constr, const std::vector<FnCallArgument>& args)
{
	if(auto c = this->typeDefnMap[cls])
		c->codegen(this);

	auto initfn = cls->getInlineInitialiser();
	iceAssert(initfn);

	auto constrfn = dcast(fir::Function, constr->codegen(this, cls).value);
	iceAssert(constrfn);

	// this is dirty, very fucking dirty!!!
	std::vector<fir::Value*> vargs;
	{
		auto copy = args;
		auto fake = util::pool<sst::RawValueExpr>(this->loc(), cls->getMutablePointerTo());
		fake->rawValue = CGResult(fir::ConstantValue::getZeroValue(cls->getMutablePointerTo()));

		//? what we are doing here is inserting a fake argument to placate `codegenAndArrangeFunctionCallArguments`, so that
		//? it does not error. this just allows us to get *THE REST* of the values in the correct order and generated appropriately,
		//? so that we can use their values and get their types below.

		copy.insert(copy.begin(), FnCallArgument(this->loc(), "this", fake, 0));
		vargs = this->codegenAndArrangeFunctionCallArguments(constr, constrfn->getType(), copy);

		// for sanity, assert that it did not change. We should not have to cast anything, and "this" is always the first
		// argument in a constructor anyway!
		iceAssert(vargs[0] == fake->rawValue.value);

		// after we are done with that shennanigans, erase the first thing, which is the 'this', which doesn't really
		// exist here!
		vargs.erase(vargs.begin());
	}


	// make a wrapper...
	auto fname = util::obfuscateIdentifier("init_wrapper", constr->id.str());
	fir::Function* wrapper_func = this->module->getFunction(fname);

	if(!wrapper_func)
	{
		auto restore = this->irb.getCurrentBlock();

		auto arglist = util::map(vargs, [](fir::Value* v) -> auto {
			return v->getType();
		});

		wrapper_func = this->module->getOrCreateFunction(fname, fir::FunctionType::get(arglist, cls), fir::LinkageType::Internal);

		fir::IRBlock* entry = this->irb.addNewBlockInFunction("entry", wrapper_func);
		this->irb.setCurrentBlock(entry);

		// make the self:
		auto selfptr = this->irb.StackAlloc(cls, "self");

		std::vector<fir::Value*> argvals = util::map(wrapper_func->getArguments(), [](auto a) -> fir::Value* {
			return a;
		});

		argvals.insert(argvals.begin(), selfptr);

		this->irb.Call(initfn, selfptr);

		this->irb.Call(constrfn, argvals);
		this->irb.Return(this->irb.ReadPtr(selfptr));

		this->irb.setCurrentBlock(restore);
	}

	if(vargs.size() != wrapper_func->getArgumentCount())
	{
		SimpleError::make(this->loc(), "mismatched number of arguments in constructor call to class '%s'; expected %d, found %d instead",
			(fir::Type*) cls, constrfn->getArgumentCount(), vargs.size())
			->append(SimpleError::make(MsgType::Note, constr->loc, "constructor was defined here:"))
			->postAndQuit();
	}

	auto ret = this->irb.Call(wrapper_func, vargs);
	this->addRAIIValue(ret);

	return ret;
}



CGResult sst::StructConstructorCall::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	this->target->codegen(cs);

	if(!this->target)
		error(this, "failed to find target type of constructor call");

	//* note: we don't need an explicit thing telling us whether we should use names or not
	//* if the first argument has no name, then we're not using names; if it has a name, then we are
	//* and ofc expect consistency, but we should have already typechecked that previously.

	StructDefn* str = dcast(StructDefn, this->target);
	if(!str) error(this, "non-struct type '%s' not supported in constructor call", this->target->id.name);

	// great. now we just make the thing.
	fir::Value* value = cs->getConstructedStructValue(str->type->toStructType(), this->arguments);

	return CGResult(value);
}






CGResult sst::ClassConstructorCall::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	this->classty->codegen(cs);

	auto cls = this->classty->type->toClassType();
	auto ret = cs->constructClassWithArguments(cls, this->target, this->arguments);

	if(fir::isRefCountedType(cls))
		cs->addRefCountedValue(ret);

	return CGResult(ret);
}



CGResult sst::BaseClassConstructorCall::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	this->classty->codegen(cs);

	auto cls = this->classty->type;
	auto self = cs->getMethodSelf();

	iceAssert(self->getType()->isClassType());

	auto selfty = self->getType()->toClassType();
	iceAssert(selfty->getBaseClass());

	auto basety = selfty->getBaseClass();

	// just do it manually here: since we already have a self pointer, we can call the base class constructor function
	// directly. plus, we are not calling the inline initialiser also.
	{
		auto constrfn = dcast(fir::Function, this->target->codegen(cs, cls).value);
		iceAssert(constrfn);

		auto copy = this->arguments;
		auto selfptr = util::pool<RawValueExpr>(this->loc, selfty->getMutablePointerTo());
		selfptr->rawValue = CGResult(cs->irb.PointerTypeCast(cs->irb.AddressOf(cs->getMethodSelf(), /* mutable: */ true),
			basety->getMutablePointerTo()));

		copy.insert(copy.begin(), FnCallArgument(this->loc, "this", selfptr, 0));

		std::vector<fir::Value*> vargs = cs->codegenAndArrangeFunctionCallArguments(this->target, constrfn->getType(), copy);

		cs->irb.Call(constrfn, vargs);
	}

	return CGResult(0);
}




































