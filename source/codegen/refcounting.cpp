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
			error(l, "Adding duplicate refcounted %s (ptr = %p, type = '%s')", kind, v, v->getType()->str());
	}

	static void _removeRC(const Location& l, fir::Value* v, std::vector<fir::Value*>* list, std::string kind)
	{
		if(auto it = std::find(list->begin(), list->end(), v); it != list->end())
			list->erase(it);

		else
			error(l, "Removing non-existant refcounted %s (ptr = %p, type = '%s')", kind, v, v->getType()->str());
	}



	void CodegenState::addRefCountedValue(fir::Value* val)
	{
		_addRC(this->loc(), val, &this->vtree->refCountedValues, "value");
	}

	void CodegenState::removeRefCountedValue(fir::Value* val)
	{
		_removeRC(this->loc(), val, &this->vtree->refCountedValues, "value");
	}


	void CodegenState::addRefCountedPointer(fir::Value* val)
	{
		_addRC(this->loc(), val, &this->vtree->refCountedPointers, "pointer");
	}

	void CodegenState::removeRefCountedPointer(fir::Value* val)
	{
		_removeRC(this->loc(), val, &this->vtree->refCountedPointers, "pointer");
	}




	std::vector<fir::Value*> CodegenState::getRefCountedValues()
	{
		return this->vtree->refCountedValues;
	}

	std::vector<fir::Value*> CodegenState::getRefCountedPointers()
	{
		return this->vtree->refCountedPointers;
	}




	void CodegenState::performRefCountingAssignment(fir::Value* lhs, CGResult rhs, bool initial)
	{
		auto rv = rhs.value;
		// auto rk = rhs.kind;

		if(lhs) iceAssert(this->isRefCountedType(lhs->getType()));
		iceAssert(this->isRefCountedType(rv->getType()));

		// if(rk == CGResult::VK::LValue)
		{
			// ok, increment the rhs refcount;
			// and decrement the lhs refcount (only if not initial)

			this->incrementRefCount(rv);

			if(!initial)
			{
				iceAssert(lhs);
				this->decrementRefCount(lhs);
			}
		}
		// else
		// {
		// 	// the rhs has already been evaluated
		// 	// as an rvalue, its refcount *SHOULD* be one
		// 	// so we don't do anything to it
		// 	// instead, decrement the left side

		// 	// to avoid double-freeing, we remove 'val' from the list of refcounted things
		// 	// since it's an rvalue, it can't be "re-referenced", so to speak.

		// 	// the issue of double-free comes up when the variable being assigned to goes out of scope, and is freed
		// 	// since they refer to the same pointer, we get a double free if the temporary expression gets freed as well.

		// 	// this->removeRefCountedValueIfExists(rhsptr);

		// 	// now we just store as usual
		// 	// if(doAssign)
		// 	// 	this->irb.CreateStore(rhs, lhsptr);


		// 	if(!initial)
		// 		this->decrementRefCount(lhs);
		// }
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
				fir::Value* mem = cs->irb.CreateExtractValue(value, { i });

				if(incr)	cs->incrementRefCount(mem);
				else		cs->decrementRefCount(mem);
			}
			else if(isStructuredAggregate(m))
			{
				fir::Value* mem = cs->irb.CreateExtractValue(value, { i });

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

			cs->irb.CreateCall1(rf, val);
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
			fir::Value* rc = cs->irb.CreateGetDynamicArrayRefCount(val);
			fir::Value* one = fir::ConstantInt::getInt64(1);

			if(incr)	rc = cs->irb.CreateAdd(rc, one);
			else		rc = cs->irb.CreateSub(rc, one);

			cs->irb.CreateSetDynamicArrayRefCount(val, rc);
		}
		else if(type->isArrayType())
		{
			// fir::ArrayType* at = type->toArrayType();
			// for(size_t i = 0; i < at->getArraySize(); i++)
			// {
			// 	fir::Value* elm = cs->irb.CreateExtractValue(type, { i });
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

			// cgi->irb.CreateCall1(rf, type);

			error("any not supported");
		}
		else
		{
			error("no: %s", type->str().c_str());
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

















