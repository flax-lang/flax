// builtin.cpp
// Copyright (c) 2014 - 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include "errors.h"
#include "codegen.h"
#include "gluecode.h"
#include "typecheck.h"

// stupid c++, u don't do it with 'using'
namespace names = strs::names;


static fir::Value* checkNullPointerOrReturnZero(cgn::CodegenState* cs, fir::Value* ptr)
{
	iceAssert(ptr->getType() == fir::Type::getNativeWordPtr());

	auto isnull = cs->irb.ICmpEQ(ptr, fir::ConstantValue::getZeroValue(fir::Type::getNativeWordPtr()));

	auto prevb = cs->irb.getCurrentBlock();
	auto deref = cs->irb.addNewBlockAfter("deref", prevb);
	auto merge = cs->irb.addNewBlockAfter("merge", deref);

	cs->irb.CondBranch(isnull, merge, deref);

	cs->irb.setCurrentBlock(deref);
	auto rc = cs->irb.ReadPtr(ptr);
	cs->irb.UnCondBranch(merge);

	cs->irb.setCurrentBlock(merge);
	auto phi = cs->irb.CreatePHINode(fir::Type::getNativeWord());
	phi->addIncoming(fir::ConstantInt::getNative(0), prevb);
	phi->addIncoming(rc, deref);

	return phi;
}



CGResult sst::BuiltinDotOp::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	auto res = this->lhs->codegen(cs);
	auto ty = res.value->getType();

	if(this->isFunctionCall)
	{
	}
	else
	{
		if(ty->isArraySliceType())
		{
			if(this->name == names::saa::FIELD_LENGTH)
				return CGResult(cs->irb.GetArraySliceLength(res.value));

			else if(this->name == names::saa::FIELD_POINTER)
				return CGResult(cs->irb.GetArraySliceData(res.value));
		}
		else if(ty->isArrayType())
		{
			if(this->name == names::saa::FIELD_LENGTH)
			{
				return CGResult(fir::ConstantInt::getNative(ty->toArrayType()->getArraySize()));
			}
			else if(this->name == names::saa::FIELD_POINTER)
			{
				// TODO: LVALUE HOLE
				if(res.value->islvalue())
				{
					auto ret = cs->irb.ConstGEP2(cs->irb.AddressOf(res.value, /* mut: */ false), 0, 0);
					return CGResult(ret);
				}
				else
				{
					error("NOT SUP");
				}
			}
		}
		else if(ty->isRangeType())
		{
			if(this->name == names::range::FIELD_BEGIN)
				return CGResult(cs->irb.GetRangeLower(res.value));

			else if(this->name == names::range::FIELD_END)
				return CGResult(cs->irb.GetRangeUpper(res.value));

			else if(this->name == names::range::FIELD_STEP)
				return CGResult(cs->irb.GetRangeStep(res.value));

		}
		else if(ty->isAnyType())
		{
			if(this->name == names::any::FIELD_TYPEID)
				return CGResult(cs->irb.GetAnyTypeID(res.value));

			else if(this->name == names::any::FIELD_REFCOUNT)
				return CGResult(checkNullPointerOrReturnZero(cs, cs->irb.GetAnyRefCountPointer(res.value)));
		}
		else if(ty->isEnumType())
		{
			if(this->name == names::enumeration::FIELD_INDEX)
			{
				return CGResult(cs->irb.GetEnumCaseIndex(res.value));
			}
			else if(this->name == names::enumeration::FIELD_VALUE)
			{
				return CGResult(cs->irb.GetEnumCaseValue(res.value));
			}
			else if(this->name == names::enumeration::FIELD_NAME)
			{
				auto namearr = ty->toEnumType()->getNameArray();
				iceAssert(namearr->islvalue());

				auto namearrptr = cs->irb.AddressOf(namearr, /* mut: */ false);
				iceAssert(namearrptr->getType()->isPointerType() && namearrptr->getType()->getPointerElementType()->isArrayType());

				auto idx = cs->irb.GetEnumCaseIndex(res.value);
				auto n = cs->irb.GEP2(namearrptr, fir::ConstantInt::getNative(0), idx);

				return CGResult(cs->irb.ReadPtr(n));
			}
		}
	}


	error(this, "no property or builtin method '%s' on type '%s'", this->name, ty);
}






