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
	fir::ConstantValue* CodegenState::unwrapConstantNumber(fir::ConstantValue* cv)
	{
		iceAssert(cv->getType()->isConstantNumberType());
		auto cn = dcast(fir::ConstantNumber, cv);
		iceAssert(cn);

		auto ty = cv->getType()->toConstantNumberType();
		iceAssert(ty);

		if(ty->isFloating())
		{
			if(ty->getMinBits() <= fir::Type::getFloat64()->getBitWidth())
				return fir::ConstantFP::getFloat64(cn->getDouble());

			else
				error("float overflow");
		}
		else
		{
			if(ty->getMinBits() < fir::Type::getNativeWord()->getBitWidth() - 1)
				return fir::ConstantInt::getNative(cn->getInt64());

			else if(!ty->isSigned() && ty->getMinBits() <= fir::Type::getNativeUWord()->getBitWidth())
				return fir::ConstantInt::getUNative(cn->getUint64());

			else
				error("int overflow");
		}
	}


	static fir::ConstantValue* _unwrapConstantNumber(CodegenState* cs, fir::ConstantNumber* num, fir::Type* target, bool isAutocast)
	{
		if(!(target->isIntegerType() || target->isFloatingPointType()))
			error(cs->loc(), "unable to cast number literal to inferred type '%s'", target);

		auto ty = num->getType()->toConstantNumberType();

		bool signConvert = false;
		if(ty->isFloating() && target->isIntegerType())
		{
			if(isAutocast) return 0;
			warn(cs->loc(), "casting floating-point literal to integer type '%s' will cause a truncation", target);
		}
		else if(target->isIntegerType() && !target->isSignedIntType() && ty->isSigned())
		{
			if(isAutocast) return 0;
			warn(cs->loc(), "casting negative literal to an unsigned integer type '%s'", target);
			signConvert = true;
		}


		if(target->toPrimitiveType()->getBitWidth() < ty->getMinBits())
		{
			// TODO: actually do what we say.
			warn(cs->loc(), "casting literal to type '%s' will cause an overflow; value will be truncated bitwise to fit",
				target);
		}

		if(signConvert)
		{
			// eg. ((size_t) -1) gives SIZET_MAX, basically.
			// so what we do, is we get the max of the target type,
			// then subtract (num - 1)

			if(target == fir::Type::getUint8())
				return fir::ConstantInt::get(target, num->getUint8());

			else if(target == fir::Type::getUint16())
				return fir::ConstantInt::get(target, num->getUint16());

			else if(target == fir::Type::getUint32())
				return fir::ConstantInt::get(target, num->getUint32());

			else if(target == fir::Type::getUint64())
				return fir::ConstantInt::get(target, num->getUint64());

			else
				error("what %s", target);
		}

		if(target == fir::Type::getFloat32())		return fir::ConstantFP::getFloat32(num->getFloat());
		else if(target == fir::Type::getFloat64())	return fir::ConstantFP::getFloat64(num->getDouble());
		else if(target == fir::Type::getInt8())		return fir::ConstantInt::get(target, num->getInt8());
		else if(target == fir::Type::getInt16())	return fir::ConstantInt::get(target, num->getInt16());
		else if(target == fir::Type::getInt32())	return fir::ConstantInt::get(target, num->getInt32());
		else if(target == fir::Type::getInt64())	return fir::ConstantInt::get(target, num->getInt64());
		else if(target == fir::Type::getUint8())	return fir::ConstantInt::get(target, num->getUint8());
		else if(target == fir::Type::getUint16())	return fir::ConstantInt::get(target, num->getUint16());
		else if(target == fir::Type::getUint32())	return fir::ConstantInt::get(target, num->getUint32());
		else if(target == fir::Type::getUint64())	return fir::ConstantInt::get(target, num->getUint64());

		else if(target == fir::Type::getNativeWord())   return fir::ConstantInt::get(target, num->getInt64());
		else if(target == fir::Type::getNativeUWord())  return fir::ConstantInt::get(target, num->getUint64());
		else										    error("unsupported type '%s'", target);
	}



	fir::ConstantValue* CodegenState::unwrapConstantNumber(fir::ConstantNumber* cv, fir::Type* target)
	{
		if(target)  return _unwrapConstantNumber(this, cv, target, false);
		else        return this->unwrapConstantNumber(cv);
	}







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
		else if(fromType->isStringType() && target == fir::Type::getInt8Ptr())
		{
			result = this->irb.PointerTypeCast(this->irb.GetSAAData(from), fir::Type::getInt8Ptr());
		}
		else if(fromType->isStringType() && target->isCharSliceType())
		{
			auto ret = this->irb.CreateValue(target);
			ret = this->irb.SetArraySliceData(ret, this->irb.GetSAAData(from));
			ret = this->irb.SetArraySliceLength(ret, this->irb.GetSAALength(from));

			result = ret;
		}
		else if(fromType->isDynamicArrayType() && target->isArraySliceType() && target->getArrayElementType() == fromType->getArrayElementType())
		{
			// ok, then
			auto ret = this->irb.CreateValue(fir::ArraySliceType::get(fromType->getArrayElementType(), target->toArraySliceType()->isMutable()));
			ret = this->irb.SetArraySliceData(ret, this->irb.GetSAAData(from));
			ret = this->irb.SetArraySliceLength(ret, this->irb.GetSAALength(from));

			result = ret;
		}
		else if(fromType->isPointerType() && target->isPointerType() && fromType->getPointerElementType()->isClassType()
			&& fromType->getPointerElementType()->toClassType()->hasParent(target->getPointerElementType()))
		{
			auto ret = this->irb.PointerTypeCast(from, target);
			result = ret;
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
		else if(target->isAnyType())
		{
			// great.
			auto fn = glue::any::generateCreateAnyWithValueFunction(this, from->getType());
			iceAssert(fn);

			result = this->irb.Call(fn, from);
		}


		if(!result)
		{
			error(this->loc(), "unsupported autocast of '%s' -> '%s'", fromType, target);
		}
		else
		{
			if(fir::isRefCountedType(result->getType()))
				this->addRefCountedValue(result);

			return result;
		}
	}

	std::pair<fir::Value*, fir::Value*> CodegenState::autoCastValueTypes(fir::Value* lhs, fir::Value* rhs)
	{
		auto lt = lhs->getType();
		auto rt = rhs->getType();
		if(lt == rt)
		{
			// if(lt->isConstantNumberType())
			// {
			// 	// well. do the sensible default, i guess.
			// 	iceAssert(rt->isConstantNumberType());

			// 	auto cnt = fir::unifyConstantTypes(lt->toConstantNumberType(), rt->toConstantNumberType());
			// 	if(cnt->isFloating())
			// 		return { this->irb.AppropriateCast(lhs, cnt), this->irb.AppropriateCast(rhs, cnt) };
			// }

			return { lhs, rhs };
		}

		// prefer to cast the void pointer to the other one, not the other way around.
		if(lt->isNullType() && rt->isPointerType())
			return std::make_pair(this->irb.PointerTypeCast(lhs, rt), rhs);

		else if(lt->isPointerType() && rt->isNullType())
			return std::make_pair(lhs, this->irb.PointerTypeCast(rhs, lt));


		/* if(lt->isConstantNumberType() && !rt->isConstantNumberType())
		{
			auto cn = dcast(fir::ConstantNumber, lhs);
			iceAssert(cn);

			auto res = _unwrapConstantNumber(this, cn, rt, true);
			if(!res)	return { lhs, rhs };
			else		return { CGResult(res), rhs };
		}
		else if(!lt->isConstantNumberType() && rt->isConstantNumberType())
		{
			auto [ l, r ] = this->autoCastValueTypes(rhs, lhs);
			return { r, l };
		}
		else  */if(lt->isIntegerType() && rt->isIntegerType() && lt->isSignedIntType() == rt->isSignedIntType())
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

		// nope...
		warn(this->loc(), "unsupported autocast of '%s' -> '%s'", lt, rt);
		return { 0, 0 };
	}
}








