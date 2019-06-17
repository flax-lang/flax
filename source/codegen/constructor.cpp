// constructor.cpp
// Copyright (c) 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include "sst.h"
#include "codegen.h"

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

		// if(names) iceAssert(i == str->getElementCount());
	}

	if(fir::isRefCountedType(str))
		this->addRefCountedValue(value);

	return value;
}



void cgn::CodegenState::constructClassWithArguments(fir::ClassType* cls, sst::FunctionDefn* constr,
	fir::Value* selfptr, const std::vector<FnCallArgument>& args, bool callInlineInit)
{
	if(auto c = this->typeDefnMap[cls])
		c->codegen(this);

	auto initfn = cls->getInlineInitialiser();
	iceAssert(initfn);

	auto constrfn = dcast(fir::Function, constr->codegen(this, cls).value);
	iceAssert(constrfn);

	// make a copy
	auto arguments = args;
	{
		auto fake = new sst::RawValueExpr(this->loc(), cls->getPointerTo());
		fake->rawValue = CGResult(selfptr);

		arguments.insert(arguments.begin(), FnCallArgument(this->loc(), "self", fake, 0));
	}


	if(arguments.size() != constrfn->getArgumentCount())
	{
		SimpleError::make(this->loc(), "mismatched number of arguments in constructor call to class '%s'; expected %d, found %d instead",
			(fir::Type*) cls, constrfn->getArgumentCount(), arguments.size())
			->append(SimpleError::make(MsgType::Note, constr->loc, "constructor was defined here:"))
			->postAndQuit();
	}

	std::vector<fir::Value*> vargs = this->codegenAndArrangeFunctionCallArguments(constr, constrfn->getType(), arguments);

	if(callInlineInit)
		this->irb.Call(initfn, selfptr);

	this->irb.Call(constrfn, vargs);
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

	auto cls = this->classty->type;
	auto self = cs->irb.CreateLValue(cls);

	cs->constructClassWithArguments(cls->toClassType(), this->target, cs->irb.AddressOf(self, true), this->arguments, true);

	// auto value = cs->irb.Dereference(self);
	if(fir::isRefCountedType(cls))
		cs->addRefCountedValue(self);

	return CGResult(self);
}



CGResult sst::BaseClassConstructorCall::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	this->classty->codegen(cs);

	auto cls = this->classty->type;
	auto self = cs->irb.AddressOf(cs->getMethodSelf(), true);

	iceAssert(self->getType()->isPointerType() && self->getType()->getPointerElementType()->isClassType());

	auto selfty = self->getType()->getPointerElementType()->toClassType();
	iceAssert(selfty->getBaseClass());

	selfty = selfty->getBaseClass();
	self = cs->irb.PointerTypeCast(self, selfty->getPointerTo());

	//* note: we don't call the inline initialiser of the base class, because the inline initialiser of our own class would've already called it.
	cs->constructClassWithArguments(cls->toClassType(), this->target, self, this->arguments, false);
	return CGResult(self);
}











