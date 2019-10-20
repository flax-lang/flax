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
		std::vector<fir::Value*> arguments = util::map(this->args, [cs](sst::Expr* e) -> fir::Value* { return e->codegen(cs).value; });
		if(this->name == names::saa::FN_CLONE)
		{
			iceAssert(arguments.empty());
			auto clonef = cgn::glue::saa_common::generateCloneFunction(cs, ty);

			auto ret = cs->irb.Call(clonef, cs->irb.CreateSliceFromSAA(res.value, false), fir::ConstantInt::getNative(0));

			iceAssert(fir::isRefCountedType(ret->getType()));
			cs->addRefCountedValue(ret);

			return CGResult(ret);
		}
		else if(this->name == names::array::FN_POP)
		{
			iceAssert(!ty->isStringType());

			if(!res->islvalue())
				error(this->lhs, "cannot call 'pop()' on an rvalue");

			else if(ty->isArrayType())
				error(this->lhs, "cannot call 'pop()' on an array type ('%s')", ty);

			auto popf = cgn::glue::array::getPopElementFromBackFunction(cs, ty);
			auto tupl = cs->irb.Call(popf, res.value, fir::ConstantCharSlice::get(this->loc.toString()));

			// tupl[0] is the new array
			// tupl[1] is the last element

			auto newarr = cs->irb.ExtractValue(tupl, { 0 });
			auto retelm = cs->irb.ExtractValue(tupl, { 1 });

			cs->irb.Store(newarr, res.value);
			return CGResult(retelm);
		}
		else if(this->name == names::saa::FN_APPEND)
		{
			iceAssert(arguments.size() == 1);

			if(!res->islvalue())
				error(this->lhs, "cannot call 'append' on an rvalue");

			auto arg = arguments[0];
			fir::Function* appendf = cgn::glue::saa_common::generateAppropriateAppendFunction(cs, ty, arg->getType());
			iceAssert(appendf);

			if(arg->getType()->isDynamicArrayType() && arg->getType() == ty)
				arg = cs->irb.CreateSliceFromSAA(arg, true);

			else if(arg->getType()->isStringType() && arg->getType() == ty)
				arg = cs->irb.CreateSliceFromSAA(arg, true);

			auto ret = cs->irb.Call(appendf, res.value, arg);

			cs->irb.Store(ret, res.value);

			return CGResult(res);
		}
	}
	else
	{
		if(ty->isStringType() || ty->isDynamicArrayType())
		{
			if(this->name == names::saa::FIELD_POINTER)
				return CGResult(cs->irb.GetSAAData(res.value));

			else if(this->name == names::saa::FIELD_LENGTH)
				return CGResult(cs->irb.GetSAALength(res.value));

			else if(this->name == names::saa::FIELD_CAPACITY)
				return CGResult(cs->irb.GetSAACapacity(res.value));

			else if(this->name == names::saa::FIELD_REFCOUNT)
			{
				return CGResult(checkNullPointerOrReturnZero(cs, cs->irb.GetSAARefCountPointer(res.value)));
			}
			else if(ty->isStringType() && this->name == names::string::FIELD_COUNT)
			{
				auto fn = cgn::glue::string::getUnicodeLengthFunction(cs);
				iceAssert(fn);

				auto ret = cs->irb.Call(fn, cs->irb.GetSAAData(res.value));
				return CGResult(ret);
			}
		}
		else if(ty->isArraySliceType())
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
				auto ret = cs->irb.ConstGEP2(res.value, 0, 0);
				return CGResult(ret);
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
				iceAssert(namearr->getType()->isPointerType() && namearr->getType()->getPointerElementType()->isArrayType());

				auto idx = cs->irb.GetEnumCaseIndex(res.value);
				auto n = cs->irb.GEP2(namearr, fir::ConstantInt::getNative(0), idx);

				return CGResult(cs->irb.ReadPtr(n));
			}
		}
	}


	error(this, "no property or builtin method '%s' on type '%s'", this->name, ty);
}






