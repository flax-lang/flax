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
		error(this, "Non-numberical type '%s' inferred for literal number", infer->str());
		// return CGResult(fir::ConstantNumber::get(this->number));

	// todo: do some proper thing
	if((this->type->isConstantNumberType() && infer) || (infer || this->type->isIntegerType() || this->type->isFloatingPointType()))
	{
		if(!infer) infer = this->type;

		if(infer->isConstantNumberType())
			error("stop playing games");

		if(!mpfr::isint(this->number) && !infer->isFloatingPointType())
			error(this, "Non floating-point type ('%s') inferred for floating-point literal", infer->str());

		return CGResult(cs->unwrapConstantNumber(this->number, infer));
	}
	else
	{
		return CGResult(fir::ConstantNumber::get(this->number));
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
				error(v, "Mismatched type for array literal; expected element type '%s', found '%s'", elmty->str(), cv->getType()->str());

			vals.push_back(cv);
		}

		// ok
		auto array = fir::ConstantArray::get(this->type, vals);
		auto ret = cs->module->createGlobalVariable(Identifier("_FV_ARR_" + std::to_string(array->id), IdKind::Name), array->getType(),
			array, true, fir::LinkageType::Internal);

		return CGResult(array, ret);
	}
	else if(this->type->isArraySliceType())
	{
		// fir::constant
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
			// ok...
			// just do a simple thing.
			auto z = fir::ConstantInt::getInt64(0);
			return CGResult(fir::ConstantDynamicArray::get(darty, fir::ConstantValue::getZeroValue(elmty->getPointerTo()), z, z));
		}

		// make a function specifically to initialise this thing

		static size_t _id = 0;

		// add one for the -1 refcount
		auto _aty = fir::ArrayType::get(elmty, this->values.size() + 1);
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
						elmty->str(), vl.value->getType()->str());
				}

				// ok, it works
				vals.push_back(vl.value);
			}

			// set the refcount to -1
			cs->irb.CreateStore(fir::ConstantInt::getInt64(-1), cs->irb.CreateConstGEP2(array, 0, 0));

			// ok -- basically unroll the loop, except there's no loop -- so we're just...
			// doing a thing.
			for(size_t i = 0; i < vals.size(); i++)
			{
				// offset by 1
				fir::Value* ptr = cs->irb.CreateConstGEP2(array, 0, i + 1);
				cs->irb.CreateStore(vals[i], ptr);
			}

			cs->irb.CreateReturnVoid();
			cs->irb.setCurrentBlock(restore);

			// ok, call the function
			cs->irb.CreateCall0(func);
		}

		// return it
		{
			auto zero = fir::ConstantInt::getInt64(0);
			auto aptr = cs->irb.CreateConstGEP2(array, 0, 1);

			auto aa = cs->irb.CreateValue(darty);
			aa = cs->irb.CreateSetDynamicArrayData(aa, aptr);
			aa = cs->irb.CreateSetDynamicArrayLength(aa, fir::ConstantInt::getInt64(this->values.size()));
			aa = cs->irb.CreateSetDynamicArrayCapacity(aa, zero);

			aa->makeImmutable();
			return CGResult(aa);
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

	error(this, "not implemented");
}





CGResult sst::LiteralNull::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	return CGResult(fir::ConstantValue::getNull());
}

CGResult sst::LiteralBool::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	return CGResult(fir::ConstantBool::get(this->value));
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
		return CGResult(stringVal);
	}
	else
	{
		return CGResult(fir::ConstantString::get(this->str));
	}
}















