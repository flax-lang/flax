// autocasting.cpp
// Copyright (c) 2014 - 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include "sst.h"
#include "errors.h"
#include "codegen.h"
#include "gluecode.h"
#include "typecheck.h"

namespace cgn
{
	// TODO: maybe merge/refactor this and the two-way autocast into one function,
	// there's a bunch of duplication here
	fir::Value* CodegenState::oneWayAutocast(fir::Value* from, fir::Type* target)
	{
		if(!from) return 0;

		auto fromType = from->getType();
		if(fromType == target) return from;

		fir::Value* result = 0;

		if(fromType->isNullType() && target->isPointerType())
		{
			result = this->irb.PointerTypeCast(from, target);
		}
		else if(fromType->isIntegerType() && target->isIntegerType() && fromType->isSignedIntType() == target->isSignedIntType()
			&& target->getBitWidth() >= fromType->getBitWidth())
		{
			result = this->irb.IntSizeCast(from, target);
		}
		else if(fromType->isPointerType() && target->isBoolType())
		{
			//* support implicit casting for null checks
			result = this->irb.ICmpNEQ(from, fir::ConstantValue::getZeroValue(fromType));
		}
		else if(fromType->isFloatingPointType() && target->isFloatingPointType() && target->getBitWidth() >= fromType->getBitWidth())
		{
			result = this->irb.FExtend(from, target);
		}
		else if(fromType->isCharSliceType() && target == fir::Type::getInt8Ptr())
		{
			result = this->irb.GetArraySliceData(from);
		}
		else if(fromType->isPointerType() && target->isPointerType() && fromType->getPointerElementType() == target->getPointerElementType()
			&& fromType->isMutablePointer() && target->isImmutablePointer())
		{
			auto ret = this->irb.PointerTypeCast(from, target);
			result = ret;
		}
		else if(fromType->isArraySliceType() && target->isVariadicArrayType() && (fromType->getArrayElementType() == target->getArrayElementType()))
		{
			//* note: we can cheat, since at the llvm level there's no mutability distinction.
			auto ret = this->irb.Bitcast(from, target);
			result = ret;
		}
		else if(fromType->isArraySliceType() && target->isArraySliceType() && (fromType->getArrayElementType() == target->getArrayElementType())
			&& fromType->toArraySliceType()->isMutable() && !target->toArraySliceType()->isMutable())
		{
			//* note: same cheat here.
			auto ret = this->irb.Bitcast(from, target);
			result = ret;
		}
		else if(fromType->isTupleType() && target->isTupleType() && fromType->toTupleType()->getElementCount() == target->toTupleType()->getElementCount())
		{
			// auto ftt = fromType->toTupleType();
			auto ttt = target->toTupleType();

			auto tuple = this->irb.CreateValue(target);

			bool failed = false;
			for(size_t i = 0; i < ttt->getElementCount(); i++)
			{
				auto res = this->oneWayAutocast(this->irb.ExtractValue(from, { i }), ttt->getElementN(i));
				if(res == 0)
				{
					failed = true;
					break;
				}

				tuple = this->irb.InsertValue(tuple, { i }, res);
			}

			if(!failed)
				result = tuple;
		}


		if(!result)
		{
			error(this->loc(), "unsupported autocast of '%s' -> '%s'", fromType, target);
		}
		else
		{
			return result;
		}
	}

	std::pair<fir::Value*, fir::Value*> CodegenState::autoCastValueTypes(fir::Value* lhs, fir::Value* rhs)
	{
		auto lt = lhs->getType();
		auto rt = rhs->getType();
		if(lt == rt)
		{
			return { lhs, rhs };
		}

		// prefer to cast the void pointer to the other one, not the other way around.
		if(lt->isNullType() && rt->isPointerType())
			return std::make_pair(this->irb.PointerTypeCast(lhs, rt), rhs);

		else if(lt->isPointerType() && rt->isNullType())
			return std::make_pair(lhs, this->irb.PointerTypeCast(rhs, lt));

		if(lt->isIntegerType() && rt->isIntegerType() && lt->isSignedIntType() == rt->isSignedIntType())
		{
			// ok, neither are constants
			// do the normal thing

			if(lt->getBitWidth() > rt->getBitWidth())
			{
				// cast rt to lt
				return { lhs, this->irb.IntSizeCast(rhs, lt) };
			}
			else if(lt->getBitWidth() < rt->getBitWidth())
			{
				return { this->irb.IntSizeCast(lhs, rt), rhs };
			}
			else
			{
				return { lhs, rhs };
			}
		}
		else if(lt->isFloatingPointType() && rt->isFloatingPointType())
		{
			// ok, neither are constants
			// do the normal thing

			if(lt->getBitWidth() > rt->getBitWidth())
			{
				// cast rt to lt
				return { lhs, this->irb.FExtend(rhs, lt) };
			}
			else if(lt->getBitWidth() < rt->getBitWidth())
			{
				return { this->irb.FExtend(lhs, rt), rhs };
			}
			else
			{
				return { lhs, rhs };
			}
		}

		// TODO: do we really want this?
		else if((lt->isFloatingPointType() && rt->isIntegerType()) || (rt->isFloatingPointType() && lt->isIntegerType()))
		{
			if(lt->isFloatingPointType())   return { lhs, this->irb.IntToFloatCast(rhs, lt) };
			else                            return { this->irb.IntToFloatCast(lhs, rt), rhs };
		}

		// nope...
		warn(this->loc(), "unsupported autocast of '%s' -> '%s'", lt, rt);
		return { 0, 0 };
	}
}








