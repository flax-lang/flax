// structs.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "errors.h"
#include "codegen.h"
#include "typecheck.h"

CGResult sst::StructDefn::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	iceAssert(this->type && this->type->isStructType());
	// cs->typeDefnMap[this->type] = this;

	for(auto nt : this->nestedTypes)
		nt->codegen(cs);

	for(auto method : this->methods)
		method->codegen(cs);

	for(auto sm : this->staticFields)
		sm->codegen(cs);

	for(auto sm : this->staticMethods)
		sm->codegen(cs);

	return CGResult(0);
}

CGResult sst::ClassDefn::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	iceAssert(this->type && this->type->isClassType());
	// cs->typeDefnMap[this->type] = this;

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

		auto f = dynamic_cast<fir::Function*>(res.value);
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

		auto self = func->getArguments()[0];

		// make sure we call the base init first.
		if(clsty->getBaseClass())
		{
			auto bii = clsty->getBaseClass()->getInlineInitialiser();
			iceAssert(bii);

			cs->irb.Call(bii, cs->irb.PointerTypeCast(self, clsty->getBaseClass()->getMutablePointerTo()));
		}

		// set our vtable
		{
			auto vtable = cs->irb.PointerTypeCast(cs->module->getOrCreateVirtualTableForClass(clsty), fir::Type::getInt8Ptr());
			cs->irb.SetVtable(self, vtable);
		}

		for(auto fd : this->fields)
		{
			if(fd->init)
			{
				auto res = fd->init->codegen(cs, fd->type);

				auto elmptr = cs->irb.GetStructMember(self, fd->id.name);
				cs->autoAssignRefCountedValue(CGResult(0, elmptr), res, true, true);
			}
			else
			{
				auto elmptr = cs->irb.GetStructMember(self, fd->id.name);
				if(fd->type->isClassType())
				{
					cs->irb.Store(cs->irb.CreateValue(fd->type), elmptr);
				}
				else
				{
					cs->autoAssignRefCountedValue(CGResult(0, elmptr), CGResult(cs->getDefaultValue(fd->type), 0, CGResult::VK::LitRValue),
						true, true);
				}
			}
		}

		cs->irb.ReturnVoid();
		this->inlineInitFunction = func;

		clsty->setInlineInitialiser(func);
	}
	cs->irb.setCurrentBlock(restore);


	return CGResult(0);
}















static CGResult getAppropriateValuePointer(cgn::CodegenState* cs, sst::Expr* user, sst::Expr* lhs, fir::Type** baseType)
{
	auto res = lhs->codegen(cs);
	auto restype = res.value->getType();

	fir::Value* retv = 0;
	fir::Value* retp = 0;

	if(restype->isStructType() || restype->isClassType())
	{
		auto t = res.value->getType();
		iceAssert(t->isStructType() || t->isClassType());

		retv = res.value;
		retp = res.pointer;

		*baseType = restype;

		// if(!retp)
		// 	warn(cs->loc(), "no pointer");
	}
	else if(restype->isTupleType())
	{
		retv = res.value;
		retp = res.pointer;

		*baseType = restype;
	}
	else if(restype->isPointerType() && (restype->getPointerElementType()->isStructType() || restype->getPointerElementType()->isClassType()))
	{
		iceAssert(res.value->getType()->getPointerElementType()->isStructType() || res.value->getType()->getPointerElementType()->isClassType());

		retv = 0;
		retp = res.value;

		*baseType = restype->getPointerElementType();
	}
	else
	{
		error(user, "Invalid type '%s' for instance dot op", restype);
	}

	return CGResult(retv, retp);
}




CGResult sst::MethodDotOp::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());


	if(auto fc = dcast(sst::FunctionCall, this->call))
	{
		// basically what we need to do is just get the pointer
		fir::Type* sty = 0;
		auto res = getAppropriateValuePointer(cs, this, this->lhs, &sty);
		if(!res.pointer)
			res.pointer = cs->irb.ImmutStackAlloc(sty, res.value);

		// then we insert it as the first argument
		auto rv = new sst::RawValueExpr(this->loc, res.pointer->getType());
		rv->rawValue = CGResult(res.pointer);

		fc->arguments.insert(fc->arguments.begin(), FnCallArgument(this->loc, "self", rv, 0));
		return fc->codegen(cs);
	}
	else if(auto ec = dcast(sst::ExprCall, this->call))
	{
		return ec->codegen(cs);
	}
	else
	{
		error(this->call, "what?");
	}
}




CGResult sst::FieldDotOp::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	if(this->isMethodRef)
		error("method ref not supported");

	fir::Type* sty = 0;
	auto res = getAppropriateValuePointer(cs, this, this->lhs, &sty);
	if(!res.pointer)
	{
		// use extractvalue.
		return CGResult(cs->irb.ExtractValueByName(res.value, this->rhsIdent), 0, CGResult::VK::RValue);
	}
	else
	{
		auto ptr = res.pointer;

		// ok, at this point it's just a normal, instance field.
		auto val = cs->irb.GetStructMember(ptr, this->rhsIdent);
		iceAssert(val);

		return CGResult(cs->irb.Load(val), val, CGResult::VK::LValue);
	}
}



CGResult sst::TupleDotOp::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	fir::Type* _sty = 0;
	auto res = getAppropriateValuePointer(cs, this, this->lhs, &_sty);

	fir::TupleType* tty = _sty->toTupleType();
	iceAssert(tty);

	// make sure something didn't somehow manage to fuck up -- we should've checked this in the typechecker.
	iceAssert(this->index < tty->getElementCount());

	// ok, if we have a pointer, then return an lvalue
	// if not, return an rvalue

	fir::Value* retv = 0;
	fir::Value* retp = 0;
	if(res.pointer)
	{
		retp = cs->irb.StructGEP(res.pointer, this->index);
		retv = cs->irb.Load(retp);
	}
	else
	{
		retv = cs->irb.ExtractValue(res.value, { this->index });
	}

	return CGResult(retv, retp, retp ? CGResult::VK::LValue : CGResult::VK::RValue);
}



CGResult cgn::CodegenState::getStructFieldImplicitly(std::string name)
{
	fir::Value* self = this->getMethodSelf();
	auto ty = self->getType();

	auto dothing = [this, name, self](auto sty) -> auto {

		if(sty->hasElementWithName(name))
		{
			// ok -- return directly from here.
			fir::Value* ptr = this->irb.GetStructMember(self, name);
			return CGResult(this->irb.Load(ptr), ptr, CGResult::VK::LValue);
		}
		else
		{
			error(this->loc(), "Type '%s' has no field named '%s'", sty->getTypeName().str(), name);
		}
	};

	if(ty->isPointerType() && ty->getPointerElementType()->isStructType())
		return dothing(ty->getPointerElementType()->toStructType());

	else if(ty->isPointerType() && ty->getPointerElementType()->isClassType())
		return dothing(ty->getPointerElementType()->toClassType());

	else
		error(this->loc(), "Invalid self type '%s' for field named '%s'", ty, name);
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




























