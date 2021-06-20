// literals.cpp
// Copyright (c) 2014 - 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include "sst.h"
#include "codegen.h"

CGResult sst::LiteralNumber::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	if(this->is_floating != this->type->isFloatingPointType())
		error(this, "TODO cannot do this");

	if(this->type->isFloatingPointType())
		return CGResult(fir::ConstantFP::get(this->type, this->floating));

	else
		return CGResult(fir::ConstantInt::get(this->type, this->integer));
}

CGResult sst::LiteralArray::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	if(this->type->isArrayType())
	{
		auto elmty = this->type->toArrayType()->getElementType();
		if(fir::isRefCountedType(elmty))
			error(this, "cannot have refcounted type in array literal");

		std::vector<fir::ConstantValue*> vals;
		for(auto v : this->values)
		{
			auto cv = dcast(fir::ConstantValue, v->codegen(cs, elmty).value);
			if(!cv)
				error(v, "constant value required in fixed array literal");

			if(cv->getType() != elmty)
				error(v, "mismatched type for array literal; expected element type '%s', found '%s'", elmty, cv->getType());

			vals.push_back(cv);
		}

		// ok
		return CGResult(fir::ConstantArray::get(this->type, vals));
	}
	else if(this->type->isDynamicArrayType() || this->type->isArraySliceType())
	{
		// ok, this can basically be anything.
		// no restrictions.

		fir::Value* returnValue = 0;
		auto elmty = this->type->getArrayElementType();

		if(this->values.empty())
		{
			// if our element type is void, and there is no infer... die.
			if(!infer || (elmty->isVoidType() && infer == 0) || (infer && infer->getArrayElementType()->isVoidType()))
				error(this, "failed to infer type for empty array literal");

			auto z = fir::ConstantInt::getNative(0);
			if(infer->isDynamicArrayType())
			{
				// ok.
				elmty = infer->getArrayElementType();
				returnValue = fir::ConstantDynamicArray::get(fir::DynamicArrayType::get(elmty),
					fir::ConstantValue::getZeroValue(elmty->getPointerTo()), z, z);
			}
			else if(infer->isArraySliceType())
			{
				elmty = infer->getArrayElementType();

				//* note: it's clearly a null pointer, so it must be immutable.
				returnValue = fir::ConstantArraySlice::get(fir::ArraySliceType::get(elmty, false),
					fir::ConstantValue::getZeroValue(elmty->getPointerTo()), z);
			}
			else
			{
				error(this, "incorrectly inferred type '%s' for empty array literal", infer);
			}
		}
		else
		{
			// make a function specifically to initialise this thing
			static size_t _id = 0;

			auto theId = _id++;

			auto _aty = fir::ArrayType::get(elmty, this->values.size());
			auto array = cs->module->createGlobalVariable(fir::Name::obfuscate("_FV_DAR_", theId),
				_aty, fir::ConstantArray::getZeroValue(_aty), false, fir::LinkageType::Internal);

			{
				auto restore = cs->irb.getCurrentBlock();

				fir::Function* func = cs->module->getOrCreateFunction(fir::Name::obfuscate("init_array", theId),
					fir::FunctionType::get({ }, fir::Type::getVoid()), fir::LinkageType::Internal);

				fir::IRBlock* entry = cs->irb.addNewBlockInFunction("entry", func);
				cs->irb.setCurrentBlock(entry);

				auto arrptr = cs->irb.AddressOf(array, true);

				std::vector<fir::Value*> vals;
				for(auto v : this->values)
				{
					auto vl = v->codegen(cs, elmty).value;
					if(vl->getType() != elmty)
						vl = cs->oneWayAutocast(vl, elmty);

					if(vl->getType() != elmty)
					{
						error(v, "mismatched type for array literal; expected element type '%s', found '%s'",
							elmty, vl->getType());
					}

					// ok, it works
					vals.push_back(vl);
				}

				// ok -- basically unroll the loop, except there's no loop -- so we're just...
				// doing a thing.
				for(size_t i = 0; i < vals.size(); i++)
				{
					// offset by 1
					fir::Value* ptr = cs->irb.ConstGEP2(arrptr, 0, i);
					cs->irb.WritePtr(vals[i], ptr);
				}

				cs->irb.ReturnVoid();
				cs->irb.setCurrentBlock(restore);

				// ok, call the function
				cs->irb.Call(func);
			}

			// return it
			if(this->type->isDynamicArrayType())
			{
				auto arrptr = cs->irb.AddressOf(array, true);

				auto aa = cs->irb.CreateValue(this->type->toDynamicArrayType());

				aa = cs->irb.SetSAAData(aa, cs->irb.ConstGEP2(arrptr, 0, 0));
				aa = cs->irb.SetSAALength(aa, fir::ConstantInt::getNative(this->values.size()));
				aa = cs->irb.SetSAACapacity(aa, fir::ConstantInt::getNative(-1));
				aa = cs->irb.SetSAARefCountPointer(aa, fir::ConstantValue::getZeroValue(fir::Type::getNativeWordPtr()));

				returnValue = aa;
			}
			else if(this->type->isArraySliceType())
			{
				auto arrptr = cs->irb.AddressOf(array, true);

				auto aa = cs->irb.CreateValue(this->type->toArraySliceType());

				aa = cs->irb.SetArraySliceData(aa, cs->irb.PointerTypeCast(cs->irb.ConstGEP2(arrptr, 0, 0), elmty->getPointerTo()));
				aa = cs->irb.SetArraySliceLength(aa, fir::ConstantInt::getNative(this->values.size()));

				returnValue = aa;
			}
			else
			{
				error(this, "what???");
			}
		}

		iceAssert(returnValue);
		cs->addRAIIOrRCValueIfNecessary(returnValue);

		return CGResult(returnValue);
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

	bool allConst = true;
	std::vector<fir::Value*> vals;

	for(size_t i = 0; i < this->values.size(); i++)
	{
		auto ty = this->type->toTupleType()->getElementN(i);
		auto vr = this->values[i]->codegen(cs, ty).value;

		if(vr->getType() != ty)
			vr = cs->oneWayAutocast(vr, ty);

		if(vr->getType() != ty)
		{
			error(this->values[i], "mismatched types in tuple element %d; expected type '%s', found type '%s'",
				i, ty, vr->getType());
		}

		allConst &= (dcast(fir::ConstantValue, vr) != nullptr);
		vals.push_back(vr);
	}

	if(allConst)
	{
		std::vector<fir::ConstantValue*> consts = zfu::map(vals, [](auto e) -> auto { return dcast(fir::ConstantValue, e); });
		return CGResult(fir::ConstantTuple::get(consts));
	}
	else
	{
		auto tup = cs->irb.CreateValue(this->type);
		for(size_t i = 0; i < vals.size(); i++)
			tup = cs->irb.InsertValue(tup, { i }, vals[i]);

		return CGResult(tup);
	}
}





CGResult sst::LiteralNull::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	fir::Value* val = 0;
	if(infer)   val = fir::ConstantValue::getZeroValue(infer);
	else        val = fir::ConstantValue::getNull();

	return CGResult(val);
}

CGResult sst::LiteralBool::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	return CGResult(fir::ConstantBool::get(this->value));
}

CGResult sst::LiteralChar::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	return CGResult(fir::ConstantInt::getInt8(static_cast<int8_t>(this->value)));
}

CGResult sst::LiteralString::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	// allow automatic coercion of string literals into i8*
	if(this->isCString || infer == fir::Type::getInt8Ptr())
	{
		// good old i8*
		fir::Value* stringVal = cs->module->createGlobalString(this->str);
		return CGResult(stringVal);
	}
	else
	{
		auto str = cs->module->createGlobalString(this->str);
		auto slc = fir::ConstantArraySlice::get(fir::Type::getCharSlice(false), str, fir::ConstantInt::getNative(this->str.length()));

		return CGResult(slc);
	}
}















