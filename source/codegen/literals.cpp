// literals.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "sst.h"
#include "codegen.h"

CGResult sst::LiteralNumber::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	if(infer && !infer->isIntegerType() && !infer->isFloatingPointType())
		error(this, "Non-numerical type '%s' inferred for literal number", infer);
		// return CGResult(fir::ConstantNumber::get(this->number));

	// todo: do some proper thing
	if((this->type->isConstantNumberType() && infer) || (infer || this->type->isIntegerType() || this->type->isFloatingPointType()))
	{
		if(!infer) infer = this->type;

		if(infer->isConstantNumberType())
			error("stop playing games");

		if(!mpfr::isint(this->number) && !infer->isFloatingPointType())
			error(this, "Non floating-point type ('%s') inferred for floating-point literal", infer);

		return CGResult(cs->unwrapConstantNumber(this->number, infer), 0, CGResult::VK::LitRValue);
	}
	else
	{
		return CGResult(fir::ConstantNumber::get(this->number), 0, CGResult::VK::LitRValue);
	}
}

CGResult sst::LiteralArray::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	if(this->type->isArrayType())
	{
		auto elmty = this->type->toArrayType()->getElementType();

		std::vector<fir::ConstantValue*> vals;
		for(auto v : this->values)
		{
			auto cv = dynamic_cast<fir::ConstantValue*>(v->codegen(cs, elmty).value);
			if(!cv)
				error(v, "Constant value required in fixed array literal");

			if(cv->getType() != elmty)
				error(v, "Mismatched type for array literal; expected element type '%s', found '%s'", elmty, cv->getType());

			vals.push_back(cv);
		}

		// ok
		auto array = fir::ConstantArray::get(this->type, vals);
		auto ret = cs->module->createGlobalVariable(Identifier("_FV_ARR_" + std::to_string(array->id), IdKind::Name), array->getType(),
			array, true, fir::LinkageType::Internal);

		return CGResult(array, ret, CGResult::VK::LitRValue);
	}
	else if(this->type->isArraySliceType())
	{
		// TODO: support this
		error(this, "Array literal cannot be coerced into an array slice");
	}
	else if(this->type->isDynamicArrayType())
	{
		// ok, this can basically be anything.
		// no restrictions.

		auto darty = this->type->toDynamicArrayType();
		auto elmty = darty->getElementType();

		if(this->values.empty())
		{
			// if our element type is void, and there is no infer... die.
			if((elmty->isVoidType() && infer == 0) || (infer && infer->getArrayElementType()->isVoidType()))
				error(this, "Failed to infer type for empty array literal");

			//! by right, if we have no values, then elmty is *supposed* to be void.
			iceAssert(elmty->isVoidType() && infer);

			if(infer->isDynamicArrayType())
			{
				// ok.
				elmty = infer->getArrayElementType();
				// error(this, "elmty = %s", elmty);

				auto z = fir::ConstantInt::getInt64(0);
				return CGResult(fir::ConstantDynamicArray::get(fir::DynamicArrayType::get(elmty), fir::ConstantValue::getZeroValue(elmty->getPointerTo()),
					z, z), 0, CGResult::VK::LitRValue);
			}
			else if(infer->isArraySliceType())
			{
				elmty = infer->getArrayElementType();

				auto z = fir::ConstantInt::getInt64(0);
				return CGResult(fir::ConstantArraySlice::get(fir::ArraySliceType::get(elmty), fir::ConstantValue::getZeroValue(elmty->getPointerTo()), z),
					0, CGResult::VK::LitRValue);
			}
			else
			{
				error(this, "Incorrectly inferred type '%s' for empty array literal", infer);
			}
		}

		// make a function specifically to initialise this thing

		static size_t _id = 0;


		auto _aty = fir::ArrayType::get(elmty, this->values.size());
		auto array = cs->module->createGlobalVariable(Identifier("_FV_DAR_" + std::to_string(_id++), IdKind::Name),
			_aty, fir::ConstantArray::getZeroValue(_aty), false, fir::LinkageType::Internal);

		{
			auto restore = cs->irb.getCurrentBlock();

			fir::Function* func = cs->module->getOrCreateFunction(Identifier("__init_array_" + std::to_string(_id - 1), IdKind::Name),
				fir::FunctionType::get({ }, fir::Type::getVoid()), fir::LinkageType::Internal);

			fir::IRBlock* entry = cs->irb.addNewBlockInFunction("entry", func);
			cs->irb.setCurrentBlock(entry);

			std::vector<fir::Value*> vals;
			for(auto v : this->values)
			{
				auto vl = v->codegen(cs, elmty);
				if(vl.value->getType() != elmty)
					vl = cs->oneWayAutocast(vl, elmty);

				if(vl.value->getType() != elmty)
				{
					error(v, "Mismatched type for array literal; expected element type '%s', found '%s'",
						elmty, vl.value->getType());
				}

				// ok, it works
				vals.push_back(vl.value);
			}

			// ok -- basically unroll the loop, except there's no loop -- so we're just...
			// doing a thing.
			for(size_t i = 0; i < vals.size(); i++)
			{
				// offset by 1
				fir::Value* ptr = cs->irb.ConstGEP2(array, 0, i);
				cs->irb.Store(vals[i], ptr);
			}

			cs->irb.ReturnVoid();
			cs->irb.setCurrentBlock(restore);

			// ok, call the function
			cs->irb.Call(func);
		}

		// return it
		{
			auto aa = cs->irb.CreateValue(darty);

			aa = cs->irb.SetDynamicArrayData(aa, cs->irb.ConstGEP2(array, 0, 0));
			aa = cs->irb.SetDynamicArrayLength(aa, fir::ConstantInt::getInt64(this->values.size()));
			aa = cs->irb.SetDynamicArrayCapacity(aa, fir::ConstantInt::getInt64(-1));
			aa = cs->irb.SetDynamicArrayRefCountPointer(aa, fir::ConstantValue::getZeroValue(fir::Type::getInt64Ptr()));

			aa->makeImmutable();
			return CGResult(aa, 0, CGResult::VK::LitRValue);
		}
	}
	else
	{
		error(this, "what?");
	}
}

CGResult sst::LiteralTuple::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	iceAssert(this->type->isTupleType());
	auto tup = cs->irb.CreateValue(this->type);

	for(size_t i = 0; i < this->values.size(); i++)
	{
		auto ty = this->type->toTupleType()->getElementN(i);
		auto vr = this->values[i]->codegen(cs, ty);

		if(vr.value->getType() != ty)
			vr = cs->oneWayAutocast(vr, ty);

		if(vr.value->getType() != ty)
		{
			error(this->values[i], "Mismatched types in tuple element %zu; expected type '%s', found type '%s'",
				i, ty, vr.value->getType());
		}

		tup = cs->irb.InsertValue(tup, { i }, vr.value);
	}

	return CGResult(tup, 0, CGResult::VK::LitRValue);
}





CGResult sst::LiteralNull::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	fir::Value* val = 0;
	if(infer)   val = fir::ConstantValue::getZeroValue(infer);
	else        val = fir::ConstantValue::getNull();

	return CGResult(val, 0, CGResult::VK::LitRValue);
}

CGResult sst::LiteralBool::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	return CGResult(fir::ConstantBool::get(this->value), 0, CGResult::VK::LitRValue);
}

CGResult sst::LiteralString::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	// allow automatic coercion of string literals into i8*
	if(this->isCString || (infer && infer == fir::Type::getInt8Ptr()))
	{
		// good old i8*
		fir::Value* stringVal = cs->module->createGlobalString(this->str);
		return CGResult(stringVal, 0, CGResult::VK::LitRValue);
	}
	else
	{
		return CGResult(fir::ConstantString::get(this->str), 0, CGResult::VK::LitRValue);
	}
}















