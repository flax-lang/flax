// type.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "sst.h"
#include "errors.h"
#include "codegen.h"
#include "typecheck.h"

#define dcast(t, v)		dynamic_cast<t*>(v)

namespace cgn
{
	fir::ConstantValue* CodegenState::unwrapConstantNumber(fir::ConstantValue* cv)
	{
		iceAssert(cv->getType()->isConstantNumberType());
		auto cn = dcast(fir::ConstantNumber, cv);
		iceAssert(cn);

		auto num = cn->getValue();
		if(mpfr::isint(num))
		{
			if(num > mpfr::mpreal(INT64_MAX) && num < mpfr::mpreal(UINT64_MAX))
				return fir::ConstantInt::getUint64(num.toULLong());

			else if(num <= mpfr::mpreal(INT64_MAX) && num >= mpfr::mpreal(INT64_MIN))
				return fir::ConstantInt::getInt64(num.toLLong());

			else
				error("overflow");
		}
		else
		{
			if(num > __DBL_MAX__)
				return fir::ConstantFP::getFloat80(num.toLDouble());

			else
				return fir::ConstantFP::getFloat64(num.toLDouble());
		}
	}


	static fir::ConstantValue* _unwrapConstantNumber(CodegenState* cs, mpfr::mpreal num, fir::Type* target, bool isAutocast)
	{
		if(!(target->isIntegerType() || target->isFloatingPointType()))
			error(cs->loc(), "Unable to cast number literal to inferred type '%s'", target->str());


		bool signConvert = false;
		if(!mpfr::isint(num) && target->isIntegerType())
		{
			if(isAutocast) return 0;
			warn(cs->loc(), "Casting floating-point literal to integer type '%s' will cause a truncation", target->str());
		}
		else if(target->isIntegerType() && !target->isSignedIntType() && num < 0)
		{
			if(isAutocast) return 0;
			warn(cs->loc(), "Casting negative literal to an unsigned integer type '%s'", target->str()), signConvert = true;
		}

		// ok, just do it
		auto _doWarn = [](Location e, fir::Type* t) {
			warn(e, "Casting literal to type '%s' will cause an overflow; resulting value will be the limit of the casted type", t->str());
		};

		if(!fir::checkLiteralFitsIntoType(target->toPrimitiveType(), num))
			_doWarn(cs->loc(), target);

		if(signConvert)
		{
			// eg. ((size_t) -1) gives SIZET_MAX, basically.
			// so what we do, is we get the max of the target type,
			// then subtract (num - 1)

			if(target == fir::Type::getUint8())
				return fir::ConstantInt::get(target, (uint8_t) (int8_t) num.toULLong());

			else if(target == fir::Type::getUint16())
				return fir::ConstantInt::get(target, (uint16_t) (int16_t) num.toULLong());

			else if(target == fir::Type::getUint32())
				return fir::ConstantInt::get(target, (uint32_t) (int32_t) num.toULLong());

			else if(target == fir::Type::getUint64())
				return fir::ConstantInt::get(target, (uint64_t) (int64_t) num.toULLong());

			else
				error("what %s", target->str());
		}

		if(target == fir::Type::getFloat32())		return fir::ConstantFP::getFloat32(num.toFloat());
		else if(target == fir::Type::getFloat64())	return fir::ConstantFP::getFloat64(num.toDouble());
		else if(target == fir::Type::getFloat80())	return fir::ConstantFP::getFloat80(num.toLDouble());
		else if(target == fir::Type::getInt8())		return fir::ConstantInt::get(target, (int8_t) num.toLLong());
		else if(target == fir::Type::getInt16())	return fir::ConstantInt::get(target, (int16_t) num.toLLong());
		else if(target == fir::Type::getInt32())	return fir::ConstantInt::get(target, (int32_t) num.toLLong());
		else if(target == fir::Type::getInt64())	return fir::ConstantInt::get(target, (int64_t) num.toLLong());
		else if(target == fir::Type::getUint8())	return fir::ConstantInt::get(target, (uint8_t) num.toULLong());
		else if(target == fir::Type::getUint16())	return fir::ConstantInt::get(target, (uint16_t) num.toULLong());
		else if(target == fir::Type::getUint32())	return fir::ConstantInt::get(target, (uint32_t) num.toULLong());
		else if(target == fir::Type::getUint64())	return fir::ConstantInt::get(target, (uint64_t) num.toULLong());
		else										error("unsupported type '%s'", target->str());
	}



	fir::ConstantValue* CodegenState::unwrapConstantNumber(mpfr::mpreal num, fir::Type* target)
	{
		return _unwrapConstantNumber(this, num, target, false);
	}







	// TODO: maybe merge/refactor this and the two-way autocast into one function,
	// there's a bunch of duplication here
	CGResult CodegenState::oneWayAutocast(const CGResult& from, fir::Type* target)
	{
		auto fromType = from.value->getType();
		if(fromType == target) return from;

		if(fromType->isConstantNumberType())
		{
			if(target->isConstantNumberType())
				error("stop playing games bitch");

			auto cn = dcast(fir::ConstantNumber, from.value);
			iceAssert(cn);

			auto res = _unwrapConstantNumber(this, cn->getValue(), target, true);
			if(!res)	return from;
			else		return CGResult(res);
		}
		else
		{
			if(fromType->isIntegerType() && target->isIntegerType() && fromType->isSignedIntType() == target->isSignedIntType()
				&& target->getBitWidth() >= fromType->getBitWidth())
			{
				return CGResult(this->irb.CreateIntSizeCast(from.value, target));
			}
			else if(fromType->isFloatingPointType() && target->isFloatingPointType() && target->getBitWidth() >= fromType->getBitWidth())
			{
				return CGResult(this->irb.CreateFExtend(from.value, target));
			}

			warn(this->loc(), "unsupported autocast of '%s' -> '%s'", fromType->str(), target->str());
			return CGResult(0);
		}
	}

	std::pair<CGResult, CGResult> CodegenState::autoCastValueTypes(const CGResult& lhs, const CGResult& rhs)
	{
		auto lt = lhs.value->getType();
		auto rt = rhs.value->getType();
		if(lt == rt || (lt->isConstantNumberType() && rt->isConstantNumberType()))
			return { lhs, rhs };


		if(lt->isConstantNumberType() && !rt->isConstantNumberType())
		{
			if(rt->isConstantNumberType())
				error("stop playing games bitch");

			auto cn = dcast(fir::ConstantNumber, lhs.value);
			iceAssert(cn);

			auto res = _unwrapConstantNumber(this, cn->getValue(), rt, true);
			if(!res)	return { lhs, rhs };
			else		return { CGResult(res), rhs };
		}
		else if(!lt->isConstantNumberType() && rt->isConstantNumberType())
		{
			auto [ l, r ] = this->autoCastValueTypes(rhs, lhs);
			return { r, l };
		}
		else if(lt->isIntegerType() && rt->isIntegerType() && lt->isSignedIntType() == rt->isSignedIntType())
		{
			// ok, neither are constants
			// do the normal thing

			if(lt->getBitWidth() > rt->getBitWidth())
			{
				// cast rt to lt
				return { lhs, CGResult(this->irb.CreateIntSizeCast(rhs.value, lt)) };
			}
			else if(lt->getBitWidth() < rt->getBitWidth())
			{
				return { CGResult(this->irb.CreateIntSizeCast(lhs.value, rt)), rhs };
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
				return { lhs, CGResult(this->irb.CreateFExtend(rhs.value, lt)) };
			}
			else if(lt->getBitWidth() < rt->getBitWidth())
			{
				return { CGResult(this->irb.CreateFExtend(lhs.value, rt)), rhs };
			}
			else
			{
				return { lhs, rhs };
			}
		}

		warn(this->loc(), "unsupported autocast of '%s' -> '%s'", lt->str(), rt->str());
		return { CGResult(0), CGResult(0) };
	}
}








