// builtin.cpp
// Copyright (c) 2014 - 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include "errors.h"
#include "codegen.h"
#include "gluecode.h"
#include "typecheck.h"

// stupid c++, u don't do it with 'using'
namespace names = strs::names;


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
			if(this->name == names::array::FIELD_LENGTH)
				return CGResult(cs->irb.GetArraySliceLength(res.value));

			else if(this->name == names::array::FIELD_POINTER)
				return CGResult(cs->irb.GetArraySliceData(res.value));
		}
		else if(ty->isArrayType())
		{
			if(this->name == names::array::FIELD_LENGTH)
			{
				return CGResult(fir::ConstantInt::getNative(ty->toArrayType()->getArraySize()));
			}
			else if(this->name == names::array::FIELD_POINTER)
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






