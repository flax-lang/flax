// classes.h
// Copyright (c) 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "errors.h"
#include "codegen.h"
#include "typecheck.h"

CGResult sst::ClassDefn::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	iceAssert(this->type && this->type->isClassType());

	std::vector<fir::Function*> meths;
	std::vector<fir::Function*> inits;


	auto clsty = this->type->toClassType();

	//* this looks stupid, but in 'setbaseclass' we update the virtual methods of the current class.
	//* since when we previously set the base class there were no virtual methods (we were still typechecking),
	//* we need to do it again.
	if(this->baseClass)
	{
		this->baseClass->codegen(cs);
		clsty->setBaseClass(clsty->getBaseClass());
	}


	for(auto method : this->methods)
	{
		auto res = method->codegen(cs);

		auto f = dcast(fir::Function, res.value);
		meths.push_back(f);

		if(method->id.name == "init")
			inits.push_back(f);

		if(method->isVirtual)
			clsty->addVirtualMethod(f);
	}



	clsty->setMethods(meths);
	clsty->setInitialiserFunctions(inits);


	for(auto sm : this->staticFields)
		sm->codegen(cs);

	for(auto sm : this->staticMethods)
		sm->codegen(cs);

	for(auto nt : this->nestedTypes)
		nt->codegen(cs);


	// basically we make a function.
	auto restore = cs->irb.getCurrentBlock();
	{
		fir::Function* func = cs->module->getOrCreateFunction(Identifier(this->id.mangled() + "_inline_init", IdKind::Name),
			fir::FunctionType::get({ this->type->getMutablePointerTo() }, fir::Type::getVoid()),
			fir::LinkageType::Internal);

		fir::IRBlock* entry = cs->irb.addNewBlockInFunction("entry", func);
		cs->irb.setCurrentBlock(entry);

		auto self = cs->irb.Dereference(func->getArguments()[0], "self");

		// make sure we call the base init first.
		if(clsty->getBaseClass())
		{
			auto bii = clsty->getBaseClass()->getInlineInitialiser();
			iceAssert(bii);

			cs->irb.Call(bii, cs->irb.PointerTypeCast(cs->irb.AddressOf(self, true), clsty->getBaseClass()->getMutablePointerTo()));
		}

		// set our vtable
		{
			auto vtable = cs->irb.PointerTypeCast(cs->irb.AddressOf(cs->module->getOrCreateVirtualTableForClass(clsty), false), fir::Type::getInt8Ptr());
			cs->irb.SetVtable(self, vtable);
		}

		for(auto fd : this->fields)
		{
			if(fd->init)
			{
				auto res = fd->init->codegen(cs, fd->type).value;
				auto elmptr = cs->irb.GetStructMember(self, fd->id.name);

				cs->autoAssignRefCountedValue(elmptr, res, true, true);
			}
			else
			{
				auto elmptr = cs->irb.GetStructMember(self, fd->id.name);
				cs->autoAssignRefCountedValue(elmptr, cs->getDefaultValue(fd->type), true, true);
			}
		}

		cs->irb.ReturnVoid();
		this->inlineInitFunction = func;

		clsty->setInlineInitialiser(func);
	}
	cs->irb.setCurrentBlock(restore);


	return CGResult(0);
}




fir::Value* cgn::CodegenState::callVirtualMethod(sst::FunctionCall* call)
{
	auto fd = dcast(sst::FunctionDefn, call->target);
	iceAssert(fd);

	auto cls = fd->parentTypeForMethod->toClassType();
	iceAssert(cls);


	if(call->isImplicitMethodCall)
	{
		iceAssert(this->isInMethodBody() && fd->parentTypeForMethod);

		auto fake = new sst::RawValueExpr(call->loc, fd->parentTypeForMethod->getPointerTo());
		fake->rawValue = CGResult(this->getMethodSelf());

		call->arguments.insert(call->arguments.begin(), FnCallArgument(call->loc, "self", fake, 0));
	}

	iceAssert(fd->type->isFunctionType());

	auto ft = fd->type->toFunctionType();
	auto args = this->codegenAndArrangeFunctionCallArguments(fd, ft, call->arguments);

	auto idx = cls->getVirtualMethodIndex(call->name, ft);
	return this->irb.CallVirtualMethod(cls, ft, idx, args);
}





















