// refcounting.cpp
// Copyright (c) 2014 - 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include "sst.h"
#include "codegen.h"
#include "gluecode.h"

namespace cgn
{
	void CodegenState::addRefCountedValue(fir::Value* val)
	{
		auto list = &this->blockPointStack.back().refCountedValues;

		if(auto it = std::find(list->begin(), list->end(), val); it == list->end())
			list->push_back(val);
		else
			error(this->loc(), "adding duplicate refcounted value (ptr = %p, type = '%s')", val, val->getType());
	}

	void CodegenState::removeRefCountedValue(fir::Value* val)
	{
		auto list = &this->blockPointStack.back().refCountedValues;

		if(auto it = std::find(list->begin(), list->end(), val); it != list->end())
			list->erase(it);
		else
			error(this->loc(), "removing non-existent refcounted value (ptr = %p, type = '%s')", val, val->getType());
	}

	std::vector<fir::Value*> CodegenState::getRefCountedValues()
	{
		return this->blockPointStack.back().refCountedValues;
	}


	void CodegenState::autoAssignRefCountedValue(fir::Value* lhs, fir::Value* rhs, bool isinit)
	{
		iceAssert(lhs && rhs);

		if(!lhs->islvalue())
			error(this->loc(), "assignment (move) to non-lvalue and non-pointer (type '%s')", lhs->getType());

		if(fir::isRefCountedType(rhs->getType()))
		{
			if(!isinit)
				this->decrementRefCount(lhs);

			if(rhs->canmove())
			{
				this->removeRefCountedValue(rhs);
			}
			else
			{
				this->incrementRefCount(rhs);
			}
		}

		// copy will do the right thing in all cases (handle non-RAII, and call move if possible)
		this->copyRAIIValue(rhs, lhs);
	}















	static bool isStructuredAggregate(fir::Type* t)
	{
		return t->isStructType() || t->isClassType() || t->isTupleType();
	}


	template <typename T>
	void doRefCountOfAggregateType(CodegenState* cs, T* type, fir::Value* value, bool incr)
	{
		size_t i = 0;
		for(auto m : type->getElements())
		{
			if(fir::isRefCountedType(m))
			{
				fir::Value* mem = cs->irb.ExtractValue(value, { i });

				if(incr)	cs->incrementRefCount(mem);
				else		cs->decrementRefCount(mem);
			}
			else if(isStructuredAggregate(m))
			{
				fir::Value* mem = cs->irb.ExtractValue(value, { i });

				if(m->isStructType())		doRefCountOfAggregateType(cs, m->toStructType(), mem, incr);
				else if(m->isClassType())	doRefCountOfAggregateType(cs, m->toClassType(), mem, incr);
				else if(m->isTupleType())	doRefCountOfAggregateType(cs, m->toTupleType(), mem, incr);
			}

			i++;
		}
	}

	static void _doRefCount(CodegenState* cs, fir::Value* val, bool incr)
	{
		auto type = val->getType();

		if(type->isStringType())
		{
			fir::Function* rf = 0;
			if(incr) rf = glue::string::getRefCountIncrementFunction(cs);
			else rf = glue::string::getRefCountDecrementFunction(cs);

			cs->irb.Call(rf, val);
		}
		else if(isStructuredAggregate(type))
		{
			auto ty = type;

			if(ty->isStructType())		doRefCountOfAggregateType(cs, ty->toStructType(), val, incr);
			else if(ty->isClassType())	doRefCountOfAggregateType(cs, ty->toClassType(), val, incr);
			else if(ty->isTupleType())	doRefCountOfAggregateType(cs, ty->toTupleType(), val, incr);
		}
		else if(type->isDynamicArrayType())
		{
			fir::Function* rf = 0;
			if(incr) rf = glue::array::getIncrementArrayRefCountFunction(cs, type->toDynamicArrayType());
			else rf = glue::array::getDecrementArrayRefCountFunction(cs, type->toDynamicArrayType());

			iceAssert(rf);
			cs->irb.Call(rf, val);
		}
		else if(type->isArrayType())
		{
			error("no array");
		}
		else if(type->isAnyType())
		{
			fir::Function* rf = 0;
			if(incr) rf = glue::any::getRefCountIncrementFunction(cs);
			else rf = glue::any::getRefCountDecrementFunction(cs);

			cs->irb.Call(rf, val);
		}
		else
		{
			error("no: '%s'", type);
		}
	}

	void CodegenState::incrementRefCount(fir::Value* val)
	{
		iceAssert(fir::isRefCountedType(val->getType()));
		_doRefCount(this, val, true);
	}

	void CodegenState::decrementRefCount(fir::Value* val)
	{
		iceAssert(fir::isRefCountedType(val->getType()));
		_doRefCount(this, val, false);
	}
}

















