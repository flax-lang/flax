// ExecutionTarget.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "../include/ir/module.h"

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

	static size_t getLargestMember(ExecutionTarget* et, StructType* t, size_t largest)
	{
		for(auto m : t->getElements())
		{
			size_t c = 0;
			if(m->isStructType())
				c = getLargestMember(et, m->toStructType(), largest);

			else
				c = et->getTypeSizeInBits(m);

			if(c > largest) largest = c;
		}

		return largest;
	}

	static void recursiveGetTypes(Type* t, std::deque<Type*>& types)
	{
		if(t->isStructType())
		{
			for(auto m : t->toStructType()->getElements())
				recursiveGetTypes(m, types);
		}
		else
		{
			types.push_back(t);
		}
	}

	static size_t recursiveGetSize(ExecutionTarget* et, Type* t, size_t largest)
	{
		if(t->isStructType())
		{
			size_t total = 0;
			size_t first = 0;

			std::deque<Type*> types;
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


	size_t ExecutionTarget::getTypeSizeInBits(Type* t)
	{
		// check.
		if(StructType* st = t->toStructType())
		{
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
		else if(ArrayType* at = t->toArrayType())
		{
			return at->getArraySize() * this->getTypeSizeInBits(at->getElementType());
		}
		else if(t->toPointerType())
		{
			return this->psize;
		}
		else if(PrimitiveType* prt = t->toPrimitiveType())
		{
			return prt->isFloatingPointType() ? prt->getFloatingPointBitWidth() : prt->getIntegerBitWidth();
		}
		else if(t->toFunctionType())
		{
			return 0;
		}
		else
		{
			if(t->isVoidType()) return 0;

			iceAssert(0 && "unsupported type");
		}
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



















