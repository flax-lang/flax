// misc.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "errors.h"
#include "codegen.h"
#include "typecheck.h"

#define dcast(t, v)		dynamic_cast<t*>(v)

namespace cgn
{
	void CodegenState::enterNamespace(std::string name)
	{
		if(auto it = this->stree->subtrees.find(name); it != this->stree->subtrees.end())
			this->stree = it->second;

		else
			error(this->loc(), "Tried to enter non-existent namespace '%s' in current scope '%s'", name.c_str(), this->stree->name.c_str());
	}

	void CodegenState::leaveNamespace()
	{
		if(!this->stree->parent)
			error(this->loc(), "Cannot leave the top-level namespace");

		this->stree = this->stree->parent;
	}







	void CodegenState::pushLoc(const Location& l)
	{
		this->locationStack.push_back(l);
	}

	void CodegenState::pushLoc(sst::Stmt* stmt)
	{
		this->locationStack.push_back(stmt->loc);
	}

	void CodegenState::popLoc()
	{
		iceAssert(this->locationStack.size() > 0);
		this->locationStack.pop_back();
	}

	Location CodegenState::loc()
	{
		iceAssert(this->locationStack.size() > 0);
		return this->locationStack.back();
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
						issigned ? std::to_string(ci->getSignedValue()) : std::to_string(ci->getUnsignedValue()));
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
			else
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
					error(this->loc(), "Floating point literal '%s' cannot fit into type '%s'", std::to_string(ci->getValue()));

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
				bool iss = lt->toPrimitiveType()->isSigned();

				if(rt == fir::Type::getFloat32())
					fits = (iss ? ci->getSignedValue() : ci->getUnsignedValue()) <= __FLT_MAX__;

				else if(rt == fir::Type::getFloat64())
					fits = (iss ? ci->getSignedValue() : ci->getUnsignedValue()) <= __DBL_MAX__;

				else if(rt == fir::Type::getFloat80())
					fits = (iss ? ci->getSignedValue() : ci->getUnsignedValue()) <= __LDBL_MAX__;

				else
					fits = true;	// TODO: probably...

				if(!fits)
				{
					error(this->loc(), "Integer literal '%s' cannot fit into type '%s'",
						iss ? std::to_string(ci->getSignedValue()) : std::to_string(ci->getUnsignedValue()));
				}

				// ok, it fits
				// make a thing
				if(iss)
					return { CGResult(fir::ConstantFP::get(rt, (long double) ci->getSignedValue())), rhs };

				else
					return { CGResult(fir::ConstantFP::get(rt, (long double) ci->getUnsignedValue())), rhs };
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












