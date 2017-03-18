// Autocasting.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include <cmath>

#include "ast.h"
#include "codegen.h"

namespace Codegen
{
	std::pair<fir::Value*, fir::Value*> CodegenInstance::attemptTypeAutoCasting(fir::Value* lhs, fir::Value* lhsptr, fir::Value* rhs,
		fir::Value* rhsptr, int* distance)
	{
		if(lhs->getType()->isTypeEqual(rhs->getType()))
		{
			if(distance) *distance = 0;
			return { lhs, rhs };
		}

		fir::Type* lty = lhs->getType();
		fir::Type* rty = rhs->getType();

		if(distance) *distance = MIN(this->getAutoCastDistance(lty, rty), this->getAutoCastDistance(rty, lty));

		// well.
		// first things first, is what we created this function to accomplish
		// ie. side-agnostic casting with literals involved.

		if(dynamic_cast<fir::ConstantValue*>(lhs) || dynamic_cast<fir::ConstantValue*>(rhs))
		{
			// if they're both constant...
			if(dynamic_cast<fir::ConstantValue*>(lhs) && dynamic_cast<fir::ConstantValue*>(rhs))
			{
				// check the types
				if(lty->isFloatingPointType() && rty->isIntegerType())
					return { lhs, this->irb.CreateIntToFloatCast(rhs, lty) };

				else if(lty->isIntegerType() && rty->isFloatingPointType())
					return { this->irb.CreateIntToFloatCast(lhs, rty), rhs };

				else if(lty->isIntegerType() && rty->isIntegerType())
				{
					// both constants... they should both be i64/u64
					// do nothing
				}
				else if(lty->isFloatingPointType() && rty->isFloatingPointType())
				{
					// same thing, shouldn't need to do anything since they're both literals
				}
			}

			// if the left side is constant
			if(fir::ConstantValue* lcv = dynamic_cast<fir::ConstantValue*>(lhs))
			{
				if(lty->isIntegerType() && rty->isIntegerType())
				{
					// try to fit.
					bool fits = false;
					if(lty->isSignedIntType())
					{
						fits = fir::checkSignedIntLiteralFitsIntoType(rty->toPrimitiveType(),
							dynamic_cast<fir::ConstantInt*>(lcv)->getSignedValue());
					}
					else
					{
						fits = fir::checkUnsignedIntLiteralFitsIntoType(rty->toPrimitiveType(),
							dynamic_cast<fir::ConstantInt*>(lcv)->getUnsignedValue());
					}

					if(fits) return { this->irb.CreateIntSizeCast(lhs, rty), rhs };
				}
				else if(lty->isFloatingPointType() && rty->isFloatingPointType())
				{
					// try to fit.
					bool fits = fir::checkFloatingPointLiteralFitsIntoType(rty->toPrimitiveType(),
						dynamic_cast<fir::ConstantFP*>(lcv)->getValue());

					if(fits)
					{
						if(rty->toPrimitiveType()->getFloatingPointBitWidth() > lty->toPrimitiveType()->getFloatingPointBitWidth())
							return { this->irb.CreateFExtend(lhs, rty), rhs };

						else
							return { this->irb.CreateFTruncate(lhs, rty), rhs };
					}
				}
				else if(lty->isFloatingPointType() && rty->isIntegerType())
				{
					// can't do anything, since the floating point is the constant.
				}
				else if(lty->isIntegerType() && rty->isFloatingPointType())
				{
					// this one we can do.
					return { this->irb.CreateIntToFloatCast(lhs, rty), rhs };
				}
				else
				{
					// do nothing
				}
			}
			else if(fir::ConstantValue* rcv = dynamic_cast<fir::ConstantValue*>(rhs))
			{
				if(lty->isIntegerType() && rty->isIntegerType())
				{
					// try to fit.
					bool fits = false;
					if(rty->isSignedIntType())
					{
						fits = fir::checkSignedIntLiteralFitsIntoType(lty->toPrimitiveType(),
							dynamic_cast<fir::ConstantInt*>(rcv)->getSignedValue());
					}
					else
					{
						fits = fir::checkUnsignedIntLiteralFitsIntoType(lty->toPrimitiveType(),
							dynamic_cast<fir::ConstantInt*>(rcv)->getUnsignedValue());
					}

					if(fits) return { lhs, this->irb.CreateIntSizeCast(rhs, lty) };
				}
				else if(lty->isFloatingPointType() && rty->isFloatingPointType())
				{
					// try to fit.
					bool fits = fir::checkFloatingPointLiteralFitsIntoType(lty->toPrimitiveType(),
						dynamic_cast<fir::ConstantFP*>(rcv)->getValue());

					if(fits)
					{
						if(rty->toPrimitiveType()->getFloatingPointBitWidth() > lty->toPrimitiveType()->getFloatingPointBitWidth())
							return { lhs, this->irb.CreateFTruncate(rhs, lty) };

						else
							return { lhs, this->irb.CreateFExtend(rhs, lty) };
					}
				}
				else if(lty->isFloatingPointType() && rty->isIntegerType())
				{
					// do it.
					return { lhs, this->irb.CreateIntToFloatCast(rhs, lty) };
				}
				else if(lty->isIntegerType() && rty->isFloatingPointType())
				{
					// can't do anything, since the floating point is the constant.
				}
				else
				{
					// do nothing
				}
			}
		}



		// neither are constants now...
		// prefer converting rhs to lhs' type

		rhs = this->autoCastType(lhs->getType(), rhs, rhsptr);

		return { lhs, rhs };
	}




	int CodegenInstance::getAutoCastDistance(fir::Type* from, fir::Type* to)
	{
		if(!from || !to)
			return -1;

		if(from->isTypeEqual(to))
			return 0;


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
				return bb >= (2 * ab) ? (bb / ab) : -1;
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
		else if(to->isVoidPointer() && to->isPointerType())
		{
			return 15;
		}
		else if(to->isAnyType())
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


		if(target->isAnyType())
		{
			return this->makeAnyFromValue(0, from, fromPtr, Ast::ValueKind::RValue).value;
		}




		if(target->isIntegerType() && from->getType()->isIntegerType()
			&& (target->toPrimitiveType()->getIntegerBitWidth() != from->getType()->toPrimitiveType()->getIntegerBitWidth()))
		{
			bool shouldCast = dist >= 0;
			fir::ConstantInt* ci = 0;
			if(dist == -1)
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

			if(dist == -1)
			{
				if((cf = dynamic_cast<fir::ConstantFP*>(from)))
				{
					shouldCast = fir::checkFloatingPointLiteralFitsIntoType(target->toPrimitiveType(), cf->getValue());
					// warn(__debugExpr, "fits: %d", shouldCast);
				}
			}

			if(shouldCast)
			{
				if(from->getType()->toPrimitiveType()->getFloatingPointBitWidth() < target->toPrimitiveType()->getFloatingPointBitWidth())
					retval = this->irb.CreateFExtend(from, target);
			}
		}

		// check if we're passing a string to a function expecting an Int8*
		else if(target == fir::Type::getInt8Ptr() && from->getType()->isStringType())
		{
			iceAssert(from);
			retval = this->irb.CreateGetStringData(from);
		}
		else if(target->isFloatingPointType() && from->getType()->isIntegerType())
		{
			// int-to-float is 10.
			retval = this->irb.CreateIntToFloatCast(from, target);
		}
		else if(target->isVoidPointer() && from->getType()->isPointerType())
		{
			// retval = fir::ConstantValue::getNullValue(target);
			retval = this->irb.CreatePointerTypeCast(from, target);
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











