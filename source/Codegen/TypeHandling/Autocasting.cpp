// Autocasting.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "codegen.h"

namespace Codegen
{
	int CodegenInstance::getAutoCastDistance(fir::Type* from, fir::Type* to)
	{
		if(!from || !to)
			return -1;

		if(from->isTypeEqual(to))
			return 0;

		if(from->isPrimitiveType() && to->isPrimitiveType())
		{
			auto fprim = from->toPrimitiveType();
			auto tprim = to->toPrimitiveType();

			if(fprim->isLiteralType() && fprim->isFloatingPointType() && tprim->isFloatingPointType())
			{
				return 1;
			}
			else if(fprim->isLiteralType() && fprim->isIntegerType() && tprim->isIntegerType())
			{
				return 1;
			}
			else if(fprim->isLiteralType() && fprim->isIntegerType() && tprim->isFloatingPointType())
			{
				return 3;
			}
		}



		int ret = 0;
		if(from->isIntegerType() && to->isIntegerType()
			&& (from->toPrimitiveType()->getIntegerBitWidth() != to->toPrimitiveType()->getIntegerBitWidth()
				|| from->toPrimitiveType()->isSigned() != to->toPrimitiveType()->isSigned()))
		{
			unsigned int ab = from->toPrimitiveType()->getIntegerBitWidth();
			unsigned int bb = to->toPrimitiveType()->getIntegerBitWidth();

			// we only allow promotion, never truncation (implicitly anyway)
			// rules:
			// u8 can fit in u16, u32, u64, i16, i32, i64
			// u16 can fit in u32, u64, i32, i64
			// u32 can fit in u64, i64
			// u64 can only fit in itself.

			bool as = from->toPrimitiveType()->isSigned();
			bool bs = to->toPrimitiveType()->isSigned();


			if(as == false && bs == true)
			{
				return bb >= 2 * ab;
			}
			else if(as == true && bs == false)
			{
				// never allow this (signed to unsigned)
				return -1;
			}

			// ok, signs are same now.
			// make sure bitsize >=.
			if(bb < ab) return -1;


			double k = bb / ab;
			if(k < 1) k = 1 / k;

			// base-change
			k = std::log(k) / std::log(2);
			ret = (int) std::round(k);

			// check for signed-ness cast.
			if(from->toPrimitiveType()->isSigned() != to->toPrimitiveType()->isSigned())
			{
				ret += 1;
			}

			return ret;
		}
		// float to float is 1
		else if(to->isFloatingPointType() && from->isFloatingPointType())
		{
			if(to->toPrimitiveType()->getFloatingPointBitWidth() > from->toPrimitiveType()->getFloatingPointBitWidth())
				return 1;

			return -1;
		}
		// check for string to int8*
		else if(to->isPointerType() && to->getPointerElementType() == fir::Type::getInt8(this->getContext())
			&& from->isStringType())
		{
			return 2;
		}
		else if(from->isPointerType() && from->getPointerElementType() == fir::Type::getInt8(this->getContext())
			&& to->isStringType())
		{
			return 2;
		}
		else if(to->isFloatingPointType() && from->isIntegerType())
		{
			// int-to-float is 10.
			return 10;
		}
		else if(to->isPointerType() && from->isVoidPointer())
		{
			return 5;
		}
		else if(from->isDynamicArrayType() && to->isPointerType()
			&& from->toDynamicArrayType()->getElementType() == to->getPointerElementType())
		{
			// try and convert all the way
			// this means T[][][][] should convert to T**** properly.

			return 5;
		}
		else if(this->isAnyType(to))
		{
			// any cast is 25.
			return 25;
		}
		else if(from->isTupleType() && to->isTupleType() && from->toTupleType()->getElementCount() == to->toTupleType()->getElementCount())
		{
			int sum = 0;
			for(size_t i = 0; i < from->toTupleType()->getElementCount(); i++)
			{
				int d = this->getAutoCastDistance(from->toTupleType()->getElementN(i), to->toTupleType()->getElementN(i));
				if(d == -1)
					return -1;		// note: make sure this is the last case

				sum += d;
			}

			return sum;
		}

		return -1;
	}

	fir::Value* CodegenInstance::autoCastType(fir::Type* target, fir::Value* from, fir::Value* fromPtr, int* distance)
	{
		if(!target || !from)
			return from;

		// casting distance for size is determined by the number of "jumps"
		// 8 -> 16 = 1
		// 8 -> 32 = 2
		// 8 -> 64 = 3
		// 16 -> 64 = 2
		// etc.

		fir::Value* retval = from;

		int dist = this->getAutoCastDistance(from->getType(), target);
		if(distance != 0)
			*distance = dist;


		if(from->getType() == target)
		{
			return from;
		}


		if(target->isIntegerType() && from->getType()->isIntegerType()
			&& (target->toPrimitiveType()->getIntegerBitWidth() != from->getType()->toPrimitiveType()->getIntegerBitWidth()
				|| from->getType()->toPrimitiveType()->isLiteralType()))
		{
			bool shouldCast = dist >= 0;
			fir::ConstantInt* ci = 0;
			if(dist == -1 || from->getType()->toPrimitiveType()->isLiteralType())
			{
				if((ci = dynamic_cast<fir::ConstantInt*>(from)))
				{
					if(ci->getType()->isSignedIntType())
					{
						shouldCast = fir::checkSignedIntLiteralFitsIntoType(target->toPrimitiveType(), ci->getSignedValue());
					}
					else
					{
						shouldCast = fir::checkUnsignedIntLiteralFitsIntoType(target->toPrimitiveType(), ci->getUnsignedValue());
					}
				}
			}

			if(shouldCast)
			{
				// if it is a literal, we need to create a new constant with a proper type
				if(from->getType()->toPrimitiveType()->isLiteralType())
				{
					if(ci)
					{
						fir::PrimitiveType* real = 0;
						if(ci->getType()->isSignedIntType())
							real = fir::PrimitiveType::getIntN(target->toPrimitiveType()->getIntegerBitWidth());

						else
							real = fir::PrimitiveType::getUintN(target->toPrimitiveType()->getIntegerBitWidth());

						from = fir::ConstantInt::get(real, ci->getSignedValue());
					}
					else
					{
						// nothing?
						from->setType(from->getType()->toPrimitiveType()->getUnliteralType());
					}

					retval = from;
				}

				// check signed to unsigned first
				if(target->toPrimitiveType()->isSigned() != from->getType()->toPrimitiveType()->isSigned())
				{
					from = this->irb.CreateIntSignednessCast(from, from->getType()->toPrimitiveType()->getOppositeSignedType());
				}

				retval = this->irb.CreateIntSizeCast(from, target);
			}
		}

		// signed to unsigned conversion
		else if(target->isIntegerType() && from->getType()->isIntegerType()
			&& (from->getType()->toPrimitiveType()->isSigned() != target->toPrimitiveType()->isSigned()))
		{
			if(!from->getType()->toPrimitiveType()->isSigned() && target->toPrimitiveType()->isSigned())
			{
				// only do it if it preserves all data.
				// we cannot convert signed to unsigned, but unsigned to signed is okay if the
				// signed bitwidth > unsigned bitwidth
				// eg. u32 -> i32 >> not okay
				//     u32 -> i64 >> okay

				// TODO: making this more like C.
				// note(behaviour): check this
				// implicit casting -- signed to unsigned of SAME BITWITH IS ALLOWED.

				if(target->toPrimitiveType()->getIntegerBitWidth() >= from->getType()->toPrimitiveType()->getIntegerBitWidth())
					retval = this->irb.CreateIntSizeCast(from, target);
			}
			else
			{
				// TODO: making this more like C.
				// note(behaviour): check this
				// implicit casting -- signed to unsigned of SAME BITWITH IS ALLOWED.

				if(target->toPrimitiveType()->getIntegerBitWidth() >= from->getType()->toPrimitiveType()->getIntegerBitWidth())
					retval = this->irb.CreateIntSizeCast(from, target);
			}
		}

		// float literals
		else if(target->isFloatingPointType() && from->getType()->isFloatingPointType())
		{
			bool shouldCast = dist >= 0;
			fir::ConstantFP* cf = 0;

			if(dist == -1 || from->getType()->toPrimitiveType()->isLiteralType())
			{
				if((cf = dynamic_cast<fir::ConstantFP*>(from)))
				{
					shouldCast = fir::checkFloatingPointLiteralFitsIntoType(target->toPrimitiveType(), cf->getValue());
					// warn(__debugExpr, "fits: %d", shouldCast);
				}
			}

			if(shouldCast)
			{
				// if it is a literal, we need to create a new constant with a proper type
				if(from->getType()->toPrimitiveType()->isLiteralType())
				{
					if(cf)
					{
						from = fir::ConstantFP::get(target, cf->getValue());
						// info(__debugExpr, "(%s / %s)", target->str().c_str(), from->getType()->str().c_str());
					}
					else
					{
						// nothing.
						from->setType(from->getType()->toPrimitiveType()->getUnliteralType());
					}

					retval = from;
				}

				if(from->getType()->toPrimitiveType()->getFloatingPointBitWidth() < target->toPrimitiveType()->getFloatingPointBitWidth())
					retval = this->irb.CreateFExtend(from, target);
			}
		}

		// check if we're passing a string to a function expecting an Int8*
		else if(target == fir::Type::getInt8Ptr() && from->getType()->isStringType())
		{
			// GEP needs a pointer
			if(fromPtr == 0) fromPtr = this->getImmutStackAllocValue(from);

			iceAssert(fromPtr);
			retval = this->irb.CreateGetStringData(fromPtr);
		}
		else if(target->isFloatingPointType() && from->getType()->isIntegerType())
		{
			// int-to-float is 10.
			retval = this->irb.CreateIntToFloatCast(from, target);
		}
		else if(target->isPointerType() && from->getType()->isVoidPointer())
		{
			// retval = fir::ConstantValue::getNullValue(target);
			retval = this->irb.CreatePointerTypeCast(from, target);
		}
		else if(target->isPointerType() && from->getType()->isDynamicArrayType()
			&& from->getType()->toDynamicArrayType()->getElementType() == target->getPointerElementType())
		{
			// get data
			iceAssert(fromPtr);
			retval = this->irb.CreateGetDynamicArrayData(fromPtr);
		}
		else if(from->getType()->isTupleType() && target->isTupleType()
			&& from->getType()->toTupleType()->getElementCount() == target->toTupleType()->getElementCount())
		{
			// somewhat complicated

			if(fromPtr == 0)
			{
				fromPtr = this->irb.CreateStackAlloc(from->getType());
				this->irb.CreateStore(from, fromPtr);
			}

			fir::Value* tuplePtr = this->getStackAlloc(target);

			for(size_t i = 0; i < from->getType()->toTupleType()->getElementCount(); i++)
			{
				fir::Value* gep = this->irb.CreateStructGEP(tuplePtr, i);
				fir::Value* fromGep = this->irb.CreateStructGEP(fromPtr, i);

				fir::Value* casted = this->autoCastType(gep->getType()->getPointerElementType(), this->irb.CreateLoad(fromGep), fromGep);

				this->irb.CreateStore(casted, gep);
			}

			retval = this->irb.CreateLoad(fromPtr);
		}



		return retval;
	}


	fir::Value* CodegenInstance::autoCastType(fir::Value* left, fir::Value* right, fir::Value* rhsPtr, int* distance)
	{
		return this->autoCastType(left->getType(), right, rhsPtr, distance);
	}
}











