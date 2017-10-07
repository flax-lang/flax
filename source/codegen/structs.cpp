// structs.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "errors.h"
#include "codegen.h"
#include "typecheck.h"

#define dcast(t, v)		dynamic_cast<t*>(v)

CGResult sst::StructDefn::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	iceAssert(this->generatedType && this->generatedType->isStructType());
	return CGResult(0);
}


static CGResult getAppropriateValuePointer(cgn::CodegenState* cs, sst::Expr* user, sst::Expr* lhs, fir::Type** baseType)
{
	auto res = lhs->codegen(cs);
	auto restype = res.value->getType();

	fir::Value* retv = 0;
	fir::Value* retp = 0;

	if(restype->isStructType())
	{
		iceAssert(res.pointer->getType()->getPointerElementType()->isStructType());

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
	else if(restype->isPointerType() && restype->getPointerElementType()->isStructType())
	{
		iceAssert(res.value->getType()->getPointerElementType()->isStructType());
		retv = 0;
		retp = res.value;

		*baseType = restype->getPointerElementType();
	}
	else
	{
		error(user, "Invalid type '%s' for instance dot op", restype->str());
	}

	return CGResult(retv, retp);
}





CGResult sst::InstanceDotOp::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	fir::Type* sty = 0;
	auto res = getAppropriateValuePointer(cs, this, this->lhs, &sty);
	if(!res.pointer)
	{
		info(this, "how?");
		error(this->lhs, "did not have pointer");
	}

	auto ptr = res.pointer;

	if(this->isMethodRef)
		error("method ref not supported");

	iceAssert(sty->toStructType()->hasElementWithName(this->rhsIdent));

	// ok, at this point it's just a normal, instance field.
	auto val = cs->irb.CreateGetStructMember(ptr, this->rhsIdent);
	iceAssert(val);

	return CGResult(cs->irb.CreateLoad(val), val, CGResult::VK::LValue);
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
		retp = cs->irb.CreateStructGEP(res.pointer, this->index);
		retv = cs->irb.CreateLoad(retp);
	}
	else
	{
		retv = cs->irb.CreateExtractValue(res.value, { this->index });
	}

	return CGResult(retv, retp, retp ? CGResult::VK::LValue : CGResult::VK::RValue);
}

















