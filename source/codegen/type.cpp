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
		if(auto ci = dcast(fir::ConstantInt, cv))
		{
			iceAssert(ci->getType()->isConstantIntType() && "doesn't need unwrapping you dolt");
			auto t = ci->getType();

			if(t->isSignedIntType())
				return fir::ConstantInt::getInt64(ci->getSignedValue());

			else
				return fir::ConstantInt::getUint64(ci->getUnsignedValue());
		}
		else if(auto cf = dcast(fir::ConstantFP, cv))
		{
			iceAssert(cf->getType()->isConstantFloatType() && "doesn't need unwrapping you dolt");
			if(cf->getValue() > __DBL_MAX__)
				return fir::ConstantFP::getFloat80(cf->getValue());

			else
				return fir::ConstantFP::getFloat64(cf->getValue());
		}
		else
		{
			error(this->loc(), "Unsupported constant value for unwrap");
		}
	}


	// TODO: maybe merge/refactor this and the two-way autocast into one function,
	// there's a bunch of duplication here
	CGResult CodegenState::oneWayAutocast(const CGResult& from, fir::Type* target)
	{
		auto fromType = from.value->getType();
		if(fromType == target) return from;

		if(fromType->isIntegerType() && target->isIntegerType())
		{
			if(fromType->isConstantIntType() && !target->isConstantIntType())
			{
				// make the right side the same as the left side
				auto ci = dcast(fir::ConstantInt, from.value);
				if(!ci)
					error(this->loc(), "Value with constant number type was not a constant value");

				bool fits = false;
				bool sgn = fromType->toPrimitiveType()->isSigned();

				if(sgn)	fits = fir::checkSignedIntLiteralFitsIntoType(target->toPrimitiveType(), ci->getSignedValue());
				else	fits = fir::checkUnsignedIntLiteralFitsIntoType(target->toPrimitiveType(), ci->getUnsignedValue());

				if(!fits)
				{
					warn(this->loc(), "Integer literal '%s' cannot fit into type '%s'",
						sgn ? std::to_string(ci->getSignedValue()) : std::to_string(ci->getUnsignedValue()), target->str());
				}

				// ok, it fits
				// make a thing
				if(sgn)	return CGResult(fir::ConstantInt::get(target, ci->getSignedValue()));
				else	return CGResult(fir::ConstantInt::get(target, ci->getUnsignedValue()));
			}
			// only autoconvert if they're the same signedness
			else if(fromType->isSignedIntType() == target->isSignedIntType() && target->getBitWidth() >= fromType->getBitWidth())
			{
				return CGResult(this->irb.CreateIntSizeCast(from.value, target));
			}
		}
		else if(fromType->isFloatingPointType() && target->isFloatingPointType())
		{
			if(fromType->isConstantFloatType() && !target->isConstantFloatType())
			{
				// make the left side the same as the right side
				auto ci = dcast(fir::ConstantFP, from.value);
				if(!ci)
					error(this->loc(), "Value with constant number type was not a constant value");

				bool fits = fir::checkFloatingPointLiteralFitsIntoType(target->toPrimitiveType(), ci->getValue());

				if(!fits)
				{
					error(this->loc(), "Floating point literal '%s' cannot fit into type '%s'", std::to_string(ci->getValue()),
						target->str());
				}

				// ok, it fits
				// make a thing
				return CGResult(fir::ConstantFP::get(target, ci->getValue()));
			}
			else if(target->getBitWidth() >= fromType->getBitWidth())
			{
				return CGResult(this->irb.CreateFExtend(from.value, target));
			}
		}
		else if(fromType->isIntegerType() && target->isFloatingPointType())
		{
			// only if the integer is a constant
			if(fromType->isConstantIntType())
			{
				// make the left side the same as the right side
				auto ci = dcast(fir::ConstantInt, from.value);
				if(!ci)
					error(this->loc(), "Value with constant number type was not a constant value");

				bool fits = false;
				bool sgn = fromType->toPrimitiveType()->isSigned();

				if(target == fir::Type::getFloat32())
				{
					if(sgn)	fits = ci->getSignedValue() <= __FLT_MAX__;
					else	fits = ci->getUnsignedValue() <= __FLT_MAX__;
				}
				else if(target == fir::Type::getFloat64())
				{
					if(sgn)	fits = ci->getSignedValue() <= __DBL_MAX__;
					else	fits = ci->getUnsignedValue() <= __DBL_MAX__;
				}
				else if(target == fir::Type::getFloat80())
				{
					if(sgn)	fits = ci->getSignedValue() <= __LDBL_MAX__;
					else	fits = ci->getUnsignedValue() <= __LDBL_MAX__;
				}
				else
				{
					fits = true;	// TODO: probably...
				}


				if(!fits)
				{
					error(this->loc(), "Integer literal '%s' cannot fit into type '%s'",
						sgn ? std::to_string(ci->getSignedValue()) : std::to_string(ci->getUnsignedValue()), target->str());
				}

				// ok, it fits
				// make a thing
				if(sgn)	return CGResult(fir::ConstantFP::get(target, (long double) ci->getSignedValue()));
				else	return CGResult(fir::ConstantFP::get(target, (long double) ci->getUnsignedValue()));
			}

			// else, no.
			// this lets us pass literals into functions taking floats without explicitly
			// (and annoyingly) specifying '1.0', while preserving some type sanity
		}

		warn(this->loc(), "unsupported autocast of '%s' -> '%s'", fromType->str(), target->str());
		return CGResult(0);
	}

	std::pair<CGResult, CGResult> CodegenState::autoCastValueTypes(const CGResult& lhs, const CGResult& rhs)
	{
		auto lt = lhs.value->getType();
		auto rt = rhs.value->getType();
		if(lt == rt)
			return { lhs, rhs };

		if(lt->isIntegerType() && rt->isIntegerType())
		{
			if(lt->isConstantIntType() && !rt->isConstantIntType())
			{
				// make the left side the same as the right side
				auto ci = dcast(fir::ConstantInt, lhs.value);
				if(!ci)
					error(this->loc(), "Value with constant number type was not a constant value");

				bool fits = false;
				bool issigned = lt->toPrimitiveType()->isSigned();
				if(issigned)
					fits = fir::checkSignedIntLiteralFitsIntoType(rt->toPrimitiveType(), ci->getSignedValue());

				else
					fits = fir::checkUnsignedIntLiteralFitsIntoType(rt->toPrimitiveType(), ci->getUnsignedValue());

				if(!fits)
				{
					error(this->loc(), "Integer literal '%s' cannot fit into type '%s'",
						issigned ? std::to_string(ci->getSignedValue()) : std::to_string(ci->getUnsignedValue()), rt->str());
				}

				// ok, it fits
				// make a thing
				if(issigned)
					return { CGResult(fir::ConstantInt::get(rt, ci->getSignedValue())), rhs };

				else
					return { CGResult(fir::ConstantInt::get(rt, ci->getUnsignedValue())), rhs };
			}
			else if(!lt->isConstantIntType() && rt->isConstantIntType())
			{
				auto [ l, r ] = this->autoCastValueTypes(rhs, lhs);
				return { r, l };
			}
			else if(lt->isConstantIntType() && rt->isConstantIntType())
			{
				// uhm.
				// do nothing, because we can't really do anything at this point
				return { lhs, rhs };
			}
			// only autoconvert if they're the same signedness
			else if(lt->isSignedIntType() == rt->isSignedIntType())
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
		}
		else if(lt->isFloatingPointType() && rt->isFloatingPointType())
		{
			if(lt->isConstantFloatType() && !rt->isConstantFloatType())
			{
				// make the left side the same as the right side
				auto ci = dcast(fir::ConstantFP, lhs.value);
				if(!ci)
					error(this->loc(), "Value with constant number type was not a constant value");

				bool fits = fir::checkFloatingPointLiteralFitsIntoType(rt->toPrimitiveType(), ci->getValue());

				if(!fits)
					error(this->loc(), "Floating point literal '%s' cannot fit into type '%s'", std::to_string(ci->getValue()), rt->str());

				// ok, it fits
				// make a thing
				return { CGResult(fir::ConstantFP::get(rt, ci->getValue())), rhs };
			}
			else if(!lt->isConstantFloatType() && rt->isConstantFloatType())
			{
				auto [ l, r ] = this->autoCastValueTypes(rhs, lhs);
				return { r, l };
			}
			else if(lt->isConstantFloatType() && rt->isConstantFloatType())
			{
				// uhm.
				// do nothing, because we can't really do anything at this point
				return { lhs, rhs };
			}
			else
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
		}
		else if(lt->isIntegerType() && rt->isFloatingPointType())
		{
			// only if the integer is a constant
			if(lt->isConstantIntType())
			{
				// make the left side the same as the right side
				auto ci = dcast(fir::ConstantInt, lhs.value);
				if(!ci)
					error(this->loc(), "Value with constant number type was not a constant value");

				bool fits = false;
				bool sgn = lt->toPrimitiveType()->isSigned();

				if(rt == fir::Type::getFloat32())
				{
					if(sgn)	fits = ci->getSignedValue() <= __FLT_MAX__;
					else	fits = ci->getUnsignedValue() <= __FLT_MAX__;
				}
				else if(rt == fir::Type::getFloat64())
				{
					if(sgn)	fits = ci->getSignedValue() <= __DBL_MAX__;
					else	fits = ci->getUnsignedValue() <= __DBL_MAX__;
				}
				else if(rt == fir::Type::getFloat80())
				{
					if(sgn)	fits = ci->getSignedValue() <= __LDBL_MAX__;
					else	fits = ci->getUnsignedValue() <= __LDBL_MAX__;
				}
				else
				{
					fits = true;	// TODO: probably...
				}


				if(!fits)
				{
					error(this->loc(), "Integer literal '%s' cannot fit into type '%s'",
						sgn ? std::to_string(ci->getSignedValue()) : std::to_string(ci->getUnsignedValue()), rt->str());
				}

				// ok, it fits
				// make a thing
				if(sgn)	return { CGResult(fir::ConstantFP::get(rt, (long double) ci->getSignedValue())), rhs };
				else	return { CGResult(fir::ConstantFP::get(rt, (long double) ci->getUnsignedValue())), rhs };
			}

			// else, no.
			// this lets us pass literals into functions taking floats without explicitly
			// (and annoyingly) specifying '1.0', while preserving some type sanity
		}
		else if(lt->isFloatingPointType() && rt->isIntegerType())
		{
			// lmao
			auto [ l, r ] = this->autoCastValueTypes(rhs, lhs);
			return { r, l };
		}


		warn(this->loc(), "unsupported autocast of '%s' -> '%s'", lt->str(), rt->str());
		return { CGResult(0), CGResult(0) };
	}
}








