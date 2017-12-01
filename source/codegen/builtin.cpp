// builtin.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "errors.h"
#include "codegen.h"
#include "typecheck.h"

#define dcast(t, v)		dynamic_cast<t*>(v)

CGResult sst::BuiltinDotOp::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	auto res = this->lhs->codegen(cs);
	auto ty = res.value->getType();

	if(this->isFunctionCall)
	{
		std::vector<fir::Value*> arguments = util::map(this->args, [cs](sst::Expr* e) -> fir::Value* { return e->codegen(cs).value; });
		if(this->name == "clone")
		{
			iceAssert(arguments.empty());
			auto clonef = cgn::glue::array::getCloneFunction(cs, ty);

			auto ret = cs->irb.CreateCall2(clonef, res.value, fir::ConstantInt::getInt64(0));
			return CGResult(ret, 0, CGResult::VK::LitRValue);
		}
		else if(this->name == "back")
		{
			auto ptr = cs->irb.CreateGetDynamicArrayData(res.value);
			auto idx = cs->irb.CreateSub(cs->irb.CreateGetDynamicArrayLength(res.value), fir::ConstantInt::getInt64(1));

			ptr = cs->irb.CreatePointerAdd(ptr, idx);
			return CGResult(cs->irb.CreateLoad(ptr));
		}
		else if(this->name == "pop")
		{
			if(res.kind != CGResult::VK::LValue)
				error(this->lhs, "Cannot call 'pop()' on an rvalue");

			auto popf = cgn::glue::array::getPopElementFromBackFunction(cs, ty->toDynamicArrayType());
			auto tupl = cs->irb.CreateCall2(popf, res.value, fir::ConstantString::get(this->loc.toString()));

			// tupl[0] is the new array
			// tupl[1] is the last element

			auto newarr = cs->irb.CreateExtractValue(tupl, { 0 });
			auto retelm = cs->irb.CreateExtractValue(tupl, { 1 });

			//* over here, we don't need to worry about ref-counting; lhs is an l-value, meaning that we stored a
			//* refcounted *pointer*. Since the pointer doesn't change, only the value stored within, we don't
			//* need any special-case handling here.

			//* there are cases (ie. function arguments) where we are an lvalue, but we don't have a pointer.
			//* in those cases, we just don't store anything.
			if(res.pointer)
				cs->irb.CreateStore(newarr, res.pointer);

			return CGResult(retelm);
		}
	}
	else
	{
		if(ty->isStringType())
		{
			if(this->name == "length")
			{
				return CGResult(cs->irb.CreateGetStringLength(res.value));
			}
			if(this->name == "count")
			{
				auto fn = cgn::glue::string::getUnicodeLengthFunction(cs);
				iceAssert(fn);

				auto ret = cs->irb.CreateCall1(fn, cs->irb.CreateGetStringData(res.value));
				return CGResult(ret);
			}
			else if(this->name == "ptr")
			{
				return CGResult(cs->irb.CreateGetStringData(res.value));
			}
			else if(this->name == "rc")
			{
				return CGResult(cs->irb.CreateGetStringRefCount(res.value));
			}
		}
		else if(ty->isDynamicArrayType())
		{
			if(this->name == "length" || this->name == "count")
			{
				return CGResult(cs->irb.CreateGetDynamicArrayLength(res.value));
			}
			if(this->name == "capacity")
			{
				return CGResult(cs->irb.CreateGetDynamicArrayCapacity(res.value));
			}
			else if(this->name == "ptr")
			{
				return CGResult(cs->irb.CreateGetDynamicArrayData(res.value));
			}
			else if(this->name == "rc")
			{
				return CGResult(cs->irb.CreateGetDynamicArrayRefCount(res.value));
			}
		}
		else if(ty->isArraySliceType())
		{
			if(this->name == "length" || this->name == "count")
			{
				return CGResult(cs->irb.CreateGetArraySliceLength(res.value));
			}
			else if(this->name == "ptr")
			{
				return CGResult(cs->irb.CreateGetArraySliceData(res.value));
			}
		}
		else if(ty->isArrayType())
		{
			if(this->name == "length" || this->name == "count")
			{
				return CGResult(fir::ConstantInt::getInt64(ty->toArrayType()->getArraySize()));
			}
			else if(this->name == "ptr")
			{
				iceAssert(res.pointer);
				auto ret = cs->irb.CreateConstGEP2(res.pointer, 0, 0);
				return CGResult(ret);
			}
		}
		else if(ty->isEnumType())
		{
			if(this->name == "index")
			{
				return CGResult(cs->irb.CreateGetEnumCaseIndex(res.value));
			}
			else if(this->name == "value")
			{
				return CGResult(cs->irb.CreateGetEnumCaseValue(res.value));
			}
			else if(this->name == "name")
			{
				auto namearr = ty->toEnumType()->getNameArray();
				iceAssert(namearr->getType()->isPointerType() && namearr->getType()->getPointerElementType()->isArrayType());

				auto idx = cs->irb.CreateGetEnumCaseIndex(res.value);
				auto n = cs->irb.CreateGEP2(namearr, fir::ConstantInt::getInt64(0), idx);

				return CGResult(cs->irb.CreateLoad(n));
			}
		}
	}


	error(this, "No such property '%s' on type '%s'", this->name, ty);
}






