// ExecutionTarget.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ir/module.h"

namespace fir
{
	// todo: specify this too
	#define BITS_PER_BYTE 8

	ExecutionTarget::ExecutionTarget(size_t ptrSize, size_t byteSize, size_t shortSize, size_t intSize, size_t longSize)
	{
		this->psize = ptrSize;
		this->bsize = byteSize;
		this->ssize = shortSize;
		this->isize = intSize;
		this->lsize = longSize;
	}

	size_t ExecutionTarget::getBitsPerByte()
	{
		return BITS_PER_BYTE;
	}

	size_t ExecutionTarget::getPointerWidthInBits()
	{
		return this->psize;
	}

	Type* ExecutionTarget::getPointerSizedIntegerType()
	{
		return fir::PrimitiveType::getUintN(this->psize);
	}

	#if 0
	static size_t getLargestMember(ExecutionTarget* et, Type* t, size_t largest)
	{
		std::vector<Type*> ts;
		if(t->isStructType())		ts = t->toStructType()->getElements();
		else if(t->isClassType())	ts = t->toClassType()->getElements();
		else if(t->isTupleType())	ts = t->toTupleType()->getElements();

		for(auto m : ts)
		{
			size_t c = 0;
			if(m->isStructType() || m->isClassType() || m->isTupleType())
				c = getLargestMember(et, m, largest);

			else
				c = et->getTypeSizeInBits(m);

			if(c > largest) largest = c;
		}

		return largest;
	}


	static void recursiveGetTypes(Type* t, std::vector<Type*>& types)
	{
		if(t->isStructType())
		{
			for(auto m : t->toStructType()->getElements())
				recursiveGetTypes(m, types);
		}
		else if(t->isClassType())
		{
			for(auto m : t->toClassType()->getElements())
				recursiveGetTypes(m, types);
		}
		else if(t->isTupleType())
		{
			for(auto m : t->toTupleType()->getElements())
				recursiveGetTypes(m, types);
		}
		else
		{
			types.push_back(t);
		}
	}

	static size_t recursiveGetSize(ExecutionTarget* et, Type* t, size_t largest)
	{
		if(t->isStructType() || t->isClassType() || t->isTupleType())
		{
			size_t total = 0;
			size_t first = 0;

			std::vector<Type*> types;
			recursiveGetTypes(t, types);

			for(auto m : types)
			{
				if(first == 0)
				{
					first = et->getTypeSizeInBits(m);
					total += first;
				}
				else
				{
					// not first.
					// check if we need pre-padding.
					size_t s = et->getTypeSizeInBits(m);
					if(total % s != 0)
						total += (total % s);

					total += s;
				}
			}

			// note: follow c++, sizeof(empty struct) == 1.
			if(first == 0) return 1;

			// do stride checking.
			if(total % first != 0)
				total += (total % first);

			return total;
		}
		else
		{
			return et->getTypeSizeInBits(t);
		}
	}
	#endif

	size_t ExecutionTarget::getTypeSizeInBits(Type* t)
	{
		// check.
		/*
		if(t->isStructType())
		{
			StructType* st = t->toStructType();
			if(st->isPackedStruct())
			{
				size_t total = 0;
				for(auto m : st->getElements())
					total += this->getTypeSizeInBits(m);

				return total;
			}
			else
			{
				return recursiveGetSize(this, st, getLargestMember(this, st, 0));
			}
		}
		else if(t->isClassType())
		{
			ClassType* ct = t->toClassType();
			return recursiveGetSize(this, ct, getLargestMember(this, ct, 0));
		}
		else if(t->isTupleType())
		{
			TupleType* tt = t->toTupleType();
			return recursiveGetSize(this, tt, getLargestMember(this, tt, 0));
		}
		else if(t->isArrayType())
		{
			ArrayType* at = t->toArrayType();
			return at->getArraySize() * this->getTypeSizeInBits(at->getElementType());
		}
		else if(t->isParameterPackType())
		{
			return this->getTypeSizeInBits(t->toParameterPackType()->getElementType()->getPointerTo())
				+ this->getTypeSizeInBits(fir::Type::getInt64());
		}
		else if(t->isDynamicArrayType())
		{
			return this->getTypeSizeInBits(t->toDynamicArrayType()->getElementType()->getPointerTo())
				+ (2 * this->getTypeSizeInBits(fir::Type::getInt64()));
		}
		else if(t->isPointerType())
		{
			return this->psize;
		}
		else if(t->isFunctionType())
		{
			return this->getTypeSizeInBits(fir::Type::getInt8Ptr());
		}
		else if(t->isStringType())
		{
			return this->getTypeSizeInBits(fir::Type::getInt8Ptr()) + this->getTypeSizeInBits(fir::Type::getInt64());
		}
		else if(t->isCharType())
		{
			return this->getTypeSizeInBits(fir::Type::getInt8());
		}
		else */
		if(t->isPrimitiveType())
		{
			PrimitiveType* prt = t->toPrimitiveType();
			return prt->isFloatingPointType() ? prt->getFloatingPointBitWidth() : prt->getIntegerBitWidth();
		}
		else
		{
			if(t->isVoidType()) return 0;

			_error_and_exit("unsupported type: %s", t->str().c_str());
		}
	}

	size_t ExecutionTarget::getTypeSizeInBytes(fir::Type* type)
	{
		return this->getTypeSizeInBits(type) / this->getBitsPerByte();
	}


	ExecutionTarget* ExecutionTarget::getLP64()
	{
		static ExecutionTarget* existing = 0;

		if(!existing)
		{
			existing = new ExecutionTarget(8 * BITS_PER_BYTE,
											1 * BITS_PER_BYTE,
											2 * BITS_PER_BYTE,
											4 * BITS_PER_BYTE,
											8 * BITS_PER_BYTE);
		}

		return existing;
	}

	ExecutionTarget* ExecutionTarget::getILP32()
	{
		static ExecutionTarget* existing = 0;

		if(!existing)
		{
			existing = new ExecutionTarget(4 * BITS_PER_BYTE,
											1 * BITS_PER_BYTE,
											2 * BITS_PER_BYTE,
											4 * BITS_PER_BYTE,
											4 * BITS_PER_BYTE);
		}

		return existing;
	}
}



















