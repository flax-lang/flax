// refcounting.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "sst.h"
#include "codegen.h"

namespace cgn
{
	static void _addRC(const Location& l, fir::Value* v, std::vector<fir::Value*>* list, std::string kind)
	{
		if(auto it = std::find(list->begin(), list->end(), v); it == list->end())
			list->push_back(v);

		else
			error(l, "Adding duplicate refcounted %s (ptr = %p, type = '%s')", kind, v, v->getType());
	}

	static void _removeRC(const Location& l, fir::Value* v, std::vector<fir::Value*>* list, std::string kind, bool ignore)
	{
		if(auto it = std::find(list->begin(), list->end(), v); it != list->end())
			list->erase(it);

		else if(!ignore)
			error(l, "Removing non-existent refcounted %s (ptr = %p, type = '%s')", kind, v, v->getType());
	}



	void CodegenState::addRefCountedValue(fir::Value* val)
	{
		_addRC(this->loc(), val, &this->breakingPointStack.back().refCountedValues, "value");
	}

	void CodegenState::removeRefCountedValue(fir::Value* val, bool ignore)
	{
		_removeRC(this->loc(), val, &this->breakingPointStack.back().refCountedValues, "value", ignore);
	}


	void CodegenState::addRefCountedPointer(fir::Value* val)
	{
		_addRC(this->loc(), val, &this->breakingPointStack.back().refCountedPointers, "pointer");
	}

	void CodegenState::removeRefCountedPointer(fir::Value* val, bool ignore)
	{
		_removeRC(this->loc(), val, &this->breakingPointStack.back().refCountedPointers, "pointer", ignore);
	}




	std::vector<fir::Value*> CodegenState::getRefCountedValues()
	{
		return this->breakingPointStack.back().refCountedValues;
	}

	std::vector<fir::Value*> CodegenState::getRefCountedPointers()
	{
		return this->breakingPointStack.back().refCountedPointers;
	}


	void CodegenState::moveRefCountedValue(CGResult lhs, CGResult rhs, bool initial)
	{
		auto rv = rhs.value;
		auto lv = lhs.value;

		iceAssert(this->isRefCountedType(lv->getType()));
		iceAssert(this->isRefCountedType(rv->getType()));

		{
			// decrement the lhs refcount (only if not initial)

			if(!initial)
			{
				iceAssert(lv);
				this->decrementRefCount(lv);

				// then do the store
				iceAssert(lhs.pointer);
				this->irb.Store(rv, lhs.pointer);
			}

			// then, remove the rhs from any refcounting table
			// but don't change the refcount itself.
			if(rhs.kind != CGResult::VK::LitRValue)
				this->removeRefCountedValue(rv);
		}
	}

	void CodegenState::performRefCountingAssignment(CGResult lhs, CGResult rhs, bool initial)
	{
		auto rv = rhs.value;

		if(lhs.value) iceAssert(this->isRefCountedType(lhs.value->getType()));
		iceAssert(this->isRefCountedType(rv->getType()));

		{
			// ok, increment the rhs refcount;
			// and decrement the lhs refcount (only if not initial)

			this->incrementRefCount(rv);

			if(!initial)
			{
				iceAssert(lhs.value);
				this->decrementRefCount(lhs.value);

				// do the store -- if not initial.
				// avoids immut shenanigans
				iceAssert(lhs.pointer);
				this->irb.Store(rv, lhs.pointer);
			}
		}
	}















	static bool isStructuredAggregate(fir::Type* t)
	{
		return t->isStructType() || t->isClassType() || t->isTupleType();
	}


	template <typename T>
	void doRefCountOfAggregateType(CodegenState* cs, T* type, fir::Value* value, bool incr)
	{
		// iceAssert(cgi->isRefCountedType(type));

		size_t i = 0;
		for(auto m : type->getElements())
		{
			if(cs->isRefCountedType(m))
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
			// fir::ArrayType* at = type->toArrayType();
			// for(size_t i = 0; i < at->getArraySize(); i++)
			// {
			// 	fir::Value* elm = cs->irb.ExtractValue(type, { i });
			// 	iceAssert(cs->isRefCountedType(elm->getType()));

			// 	if(incr) cs->incrementRefCount(elm);
			// 	else cs->decrementRefCount(elm);
			// }

			error("no array");
		}
		else if(type->isAnyType())
		{
			// fir::Function* rf = 0;
			// if(incr) rf = glue::Any::getRefCountIncrementFunction(cgi);
			// else rf = glue::Any::getRefCountDecrementFunction(cgi);

			// cgi->irb.Call(rf, type);

			error("any not supported");
		}
		else
		{
			error("no: '%s'", type);
		}
	}



	void CodegenState::incrementRefCount(fir::Value* val)
	{
		iceAssert(this->isRefCountedType(val->getType()));
		_doRefCount(this, val, true);
	}

	void CodegenState::decrementRefCount(fir::Value* val)
	{
		iceAssert(this->isRefCountedType(val->getType()));
		_doRefCount(this, val, false);
	}





	bool CodegenState::isRefCountedType(fir::Type* type)
	{
		// strings, and structs with rc inside
		if(type->isStructType())
		{
			for(auto m : type->toStructType()->getElements())
			{
				if(this->isRefCountedType(m))
					return true;
			}

			return false;
		}
		else if(type->isClassType())
		{
			for(auto m : type->toClassType()->getElements())
			{
				if(this->isRefCountedType(m))
					return true;
			}

			return false;
		}
		else if(type->isTupleType())
		{
			for(auto m : type->toTupleType()->getElements())
			{
				if(this->isRefCountedType(m))
					return true;
			}

			return false;
		}
		else if(type->isArrayType())	// note: no slices, because slices don't own memory
		{
			return this->isRefCountedType(type->getArrayElementType());
		}
		else
		{
			return type->isStringType() || type->isAnyType() || type->isDynamicArrayType();
		}
	}
}

















