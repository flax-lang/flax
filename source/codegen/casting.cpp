// casting.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
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

		// auto num = cn->getValue();
		auto ty = cv->getType()->toConstantNumberType();
		iceAssert(ty);

		if(ty->isFloating())
		{
			if(ty->getMinBits() <= fir::Type::getFloat64()->getBitWidth())
				return fir::ConstantFP::getFloat64(cn->getValue<double>());

			else if(ty->getMinBits() <= fir::Type::getFloat80()->getBitWidth())
				return fir::ConstantFP::getFloat80(cn->getValue<long double>());

			else
				error("float overflow");
		}
		else
		{
				if(ty->getMinBits() <= fir::Type::getInt64()->getBitWidth() - 1)
					return fir::ConstantInt::getInt64(cn->getValue<int64_t>());

				// else if(ty->getMinBits() <= fir::Type::getInt128()->getBitWidth() - 1)
				// 	return fir::Type::getInt128();

				else if(ty->isSigned() && ty->getMinBits() <= fir::Type::getUint64()->getBitWidth())
					return fir::ConstantInt::getUint64(cn->getValue<uint64_t>());

				else
					error("int overflow");
		}
	}


	static fir::ConstantValue* _unwrapConstantNumber(CodegenState* cs, fir::ConstantNumber* num, fir::Type* target, bool isAutocast)
	{
		if(!(target->isIntegerType() || target->isFloatingPointType()))
			error(cs->loc(), "Unable to cast number literal to inferred type '%s'", target);

		auto ty = num->getType()->toConstantNumberType();

		bool signConvert = false;
		if(ty->isFloating() && target->isIntegerType())
		{
			if(isAutocast) return 0;
			warn(cs->loc(), "Casting floating-point literal to integer type '%s' will cause a truncation", target);
		}
		else if(target->isIntegerType() && !target->isSignedIntType() && ty->isSigned())
		{
			if(isAutocast) return 0;
			warn(cs->loc(), "Casting negative literal to an unsigned integer type '%s'", target);
			signConvert = true;
		}


		if(target->toPrimitiveType()->getBitWidth() < ty->getMinBits())
		{
			warn(cs->loc(), "Casting literal to type '%s' will cause an overflow; resulting value will be the limit of the casted type",
				target);
		}

		if(signConvert)
		{
			// eg. ((size_t) -1) gives SIZET_MAX, basically.
			// so what we do, is we get the max of the target type,
			// then subtract (num - 1)

			if(target == fir::Type::getUint8())
				return fir::ConstantInt::get(target, num->getValue<uint8_t>());

			else if(target == fir::Type::getUint16())
				return fir::ConstantInt::get(target, num->getValue<uint16_t>());

			else if(target == fir::Type::getUint32())
				return fir::ConstantInt::get(target, num->getValue<uint32_t>());

			else if(target == fir::Type::getUint64())
				return fir::ConstantInt::get(target, num->getValue<uint64_t>());

			else
				error("what %s", target);
		}

		if(target == fir::Type::getFloat32())		return fir::ConstantFP::getFloat32(num->getValue<float>());
		else if(target == fir::Type::getFloat64())	return fir::ConstantFP::getFloat64(num->getValue<double>());
		else if(target == fir::Type::getFloat80())	return fir::ConstantFP::getFloat80(num->getValue<long double>());
		else if(target == fir::Type::getInt8())		return fir::ConstantInt::get(target, num->getValue<int8_t>());
		else if(target == fir::Type::getInt16())	return fir::ConstantInt::get(target, num->getValue<int16_t>());
		else if(target == fir::Type::getInt32())	return fir::ConstantInt::get(target, num->getValue<int32_t>());
		else if(target == fir::Type::getInt64())	return fir::ConstantInt::get(target, num->getValue<int64_t>());
		else if(target == fir::Type::getUint8())	return fir::ConstantInt::get(target, num->getValue<uint8_t>());
		else if(target == fir::Type::getUint16())	return fir::ConstantInt::get(target, num->getValue<uint16_t>());
		else if(target == fir::Type::getUint32())	return fir::ConstantInt::get(target, num->getValue<uint32_t>());
		else if(target == fir::Type::getUint64())	return fir::ConstantInt::get(target, num->getValue<uint64_t>());
		else										error("unsupported type '%s'", target);
	}



	fir::ConstantValue* CodegenState::unwrapConstantNumber(fir::ConstantNumber* cv, fir::Type* target)
	{
		return _unwrapConstantNumber(this, cv, target, false);
	}







	// TODO: maybe merge/refactor this and the two-way autocast into one function,
	// there's a bunch of duplication here
	CGResult CodegenState::oneWayAutocast(const CGResult& from, fir::Type* target)
	{
		auto fromType = from.value->getType();
		if(fromType == target) return from;

		if(fromType->isNullType() && target->isPointerType())
		{
			return CGResult(this->irb.PointerTypeCast(from.value, target));
		}
		else if(fromType->isIntegerType() && target->isIntegerType() && fromType->isSignedIntType() == target->isSignedIntType()
			&& target->getBitWidth() >= fromType->getBitWidth())
		{
			return CGResult(this->irb.IntSizeCast(from.value, target));
		}
		else if(fromType->isPointerType() && target->isBoolType())
		{
			//* support implicit casting for null checks
			return CGResult(this->irb.ICmpNEQ(from.value, fir::ConstantValue::getZeroValue(fromType)));
		}
		else if(fromType->isFloatingPointType() && target->isFloatingPointType() && target->getBitWidth() >= fromType->getBitWidth())
		{
			return CGResult(this->irb.FExtend(from.value, target));
		}
		else if(fromType->isCharSliceType() && target == fir::Type::getInt8Ptr())
		{
			return CGResult(this->irb.GetArraySliceData(from.value));
		}
		else if(fromType->isStringType() && target == fir::Type::getInt8Ptr())
		{
			return CGResult(this->irb.GetSAAData(from.value));
		}
		else if(fromType->isStringType() && target->isCharSliceType())
		{
			auto ret = this->irb.CreateValue(target);
			ret = this->irb.SetArraySliceData(ret, this->irb.GetSAAData(from.value));
			ret = this->irb.SetArraySliceLength(ret, this->irb.GetSAALength(from.value));

			return CGResult(ret);
		}
		else if(fromType->isDynamicArrayType() && target->isArraySliceType() && target->getArrayElementType() == fromType->getArrayElementType())
		{
			// ok, then
			auto ret = this->irb.CreateValue(fir::ArraySliceType::get(fromType->getArrayElementType(), target->toArraySliceType()->isMutable()));
			ret = this->irb.SetArraySliceData(ret, this->irb.GetSAAData(from.value));
			ret = this->irb.SetArraySliceLength(ret, this->irb.GetSAALength(from.value));

			return CGResult(ret);
		}
		else if(fromType->isPointerType() && target->isPointerType() && fromType->getPointerElementType()->isClassType()
			&& fromType->getPointerElementType()->toClassType()->isInParentHierarchy(target->getPointerElementType()))
		{
			auto ret = this->irb.PointerTypeCast(from.value, target);
			return CGResult(ret);
		}
		else if(fromType->isPointerType() && target->isPointerType() && fromType->getPointerElementType() == target->getPointerElementType()
			&& fromType->isMutablePointer() && target->isImmutablePointer())
		{
			auto ret = this->irb.PointerTypeCast(from.value, target);
			return CGResult(ret);
		}
		else if(fromType->isArraySliceType() && target->isVariadicArrayType() && (fromType->getArrayElementType() == target->getArrayElementType()))
		{
			//* note: we can cheat, since at the llvm level there's no mutability distinction.
			auto ret = this->irb.Bitcast(from.value, target);
			return CGResult(ret);
		}
		else if(fromType->isArraySliceType() && target->isArraySliceType() && (fromType->getArrayElementType() == target->getArrayElementType())
			&& fromType->toArraySliceType()->isMutable() && !target->toArraySliceType()->isMutable())
		{
			//* note: same cheat here.
			auto ret = this->irb.Bitcast(from.value, target);
			return CGResult(ret);
		}
		else if(fromType->isTupleType() && target->isTupleType() && fromType->toTupleType()->getElementCount() == target->toTupleType()->getElementCount())
		{
			// auto ftt = fromType->toTupleType();
			auto ttt = target->toTupleType();

			auto tuple = this->irb.CreateValue(target);

			for(size_t i = 0; i < ttt->getElementCount(); i++)
			{
				auto res = this->oneWayAutocast(CGResult(this->irb.ExtractValue(from.value, { i })), ttt->getElementN(i));
				if(res.value == 0) goto LABEL_failure;

				tuple = this->irb.InsertValue(tuple, { i }, res.value);
			}

			return CGResult(tuple);
		}
		else if(target->isAnyType())
		{
			// great.
			auto fn = glue::any::generateCreateAnyWithValueFunction(this, from.value->getType());
			iceAssert(fn);

			return CGResult(this->irb.Call(fn, from.value));
		}

		// nope.
		//! ACHTUNG !
		//* ew, goto.
		LABEL_failure:
		error(this->loc(), "unsupported autocast of '%s' -> '%s'", fromType, target);
		return CGResult(0);
	}

	std::pair<CGResult, CGResult> CodegenState::autoCastValueTypes(const CGResult& lhs, const CGResult& rhs)
	{
		auto lt = lhs.value->getType();
		auto rt = rhs.value->getType();
		if(lt == rt)
		{
			return { lhs, rhs };
		}

		// prefer to cast the void pointer to the other one, not the other way around.
		if(lt->isNullType() && rt->isPointerType())
			return std::make_pair(CGResult(this->irb.PointerTypeCast(lhs.value, rt)), CGResult(rhs.value));

		else if(lt->isPointerType() && rt->isNullType())
			return std::make_pair(CGResult(lhs.value), CGResult(this->irb.PointerTypeCast(rhs.value, lt)));


		/* if(lt->isConstantNumberType() && !rt->isConstantNumberType())
		{
			auto cn = dcast(fir::ConstantNumber, lhs.value);
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
				return { lhs, CGResult(this->irb.IntSizeCast(rhs.value, lt)) };
			}
			else if(lt->getBitWidth() < rt->getBitWidth())
			{
				return { CGResult(this->irb.IntSizeCast(lhs.value, rt)), rhs };
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
				return { lhs, CGResult(this->irb.FExtend(rhs.value, lt)) };
			}
			else if(lt->getBitWidth() < rt->getBitWidth())
			{
				return { CGResult(this->irb.FExtend(lhs.value, rt)), rhs };
			}
			else
			{
				return { lhs, rhs };
			}
		}

		// nope...
		warn(this->loc(), "unsupported autocast of '%s' -> '%s'", lt, rt);
		return { CGResult(0), CGResult(0) };
	}
}








