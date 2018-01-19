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

	for(auto method : this->methods)
		method->codegen(cs);

	for(auto nt : this->nestedTypes)
		nt->codegen(cs);

	return CGResult(0);
}

CGResult sst::ClassDefn::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	iceAssert(this->type && this->type->isClassType());

	std::vector<fir::Function*> meths;

	for(auto method : this->methods)
	{
		auto f = dynamic_cast<fir::Function*>(method->codegen(cs).value);
		meths.push_back(f);
	}

	for(auto sm : this->staticFields)
		sm->codegen(cs);

	for(auto sm : this->staticMethods)
		sm->codegen(cs);

	for(auto nt : this->nestedTypes)
		nt->codegen(cs);

	this->type->toClassType()->setMethods(meths);

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

		fc->arguments.insert(fc->arguments.begin(), FnCallArgument(this->loc, "self", rv));
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














