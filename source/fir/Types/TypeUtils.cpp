// TypeUtils.cpp
// Copyright (c) 2019, zhiayang
// Licensed under the Apache License Version 2.0.

#include "errors.h"
#include "ir/type.h"
#include "ir/function.h"

namespace fir
{
	static bool _checkTypeVariance(Type* base, Type* derv, bool contra, bool trait)
	{
		if(base == derv)
			return true;

		// for now, we only support inheritance and stuff on pointers.
		if(!base->isPointerType() || !derv->isPointerType())
			return false;

		auto baseelm = base->getPointerElementType();
		auto dervelm = derv->getPointerElementType();

		if(baseelm->isClassType() && dervelm->isClassType())
		{
			// if contravariant, then derv must be more general than base.
			if(contra)  return baseelm->toClassType()->hasParent(dervelm);
			else        return dervelm->toClassType()->hasParent(baseelm);
		}
		else
		{
			// if contra, check if base implements derv as a trait
			// else check if derv implements base as a trait. of course, if the thing that
			// is supposed to be a trait isn't a trait, it doesn't work.
			if(contra)
			{
				// if contra *BUT* we are checking traits, then we should allow the case where the
				// derived type is covariant (ie. implements the trait!)

				if(trait)
				{
					// TODO: this might not be correct?
					std::swap(baseelm, dervelm);
				}

				if(!dervelm->isTraitType())
					return false;

				return (baseelm->isStructType() && baseelm->toStructType()->implementsTrait(dervelm->toTraitType()))
					|| (dervelm->isStructType() && dervelm->toStructType()->implementsTrait(baseelm->toTraitType()));
			}
			else
			{
				if(!baseelm->isTraitType())
					return false;

				return (dervelm->isStructType() && dervelm->toStructType()->implementsTrait(baseelm->toTraitType()))
					|| (baseelm->isStructType() && baseelm->toStructType()->implementsTrait(dervelm->toTraitType()));
			}
		}
	}

	bool areTypesCovariant(Type* base, Type* derv)
	{
		return _checkTypeVariance(base, derv, false, false);
	}

	bool areTypesContravariant(Type* base, Type* derv, bool traitChecking)
	{
		return _checkTypeVariance(base, derv, true, traitChecking);
	}

	bool areTypeListsContravariant(const std::vector<Type*>& base, const std::vector<Type*>& derv, bool traitChecking)
	{
		// parameters must be contravariant, ie. fn must take more general types than base
		// return type must be covariant, ie. fn must return a more specific type than base.

		// duh
		if(base.size() != derv.size())
			return false;

		// drop the first argument.
		for(auto [ b, d ] : util::zip(base, derv))
		{
			if(!areTypesContravariant(b, d, traitChecking))
				return false;
		}

		return true;
	}

	bool areMethodsVirtuallyCompatible(FunctionType* base, FunctionType* fn, bool traitChecking)
	{
		debuglogln("check %s, %s", base, fn);
		bool ret = areTypeListsContravariant(util::drop(base->getArgumentTypes(), 1),
			util::drop(fn->getArgumentTypes(), 1), traitChecking);

		if(!ret)
			return false;

		auto baseRet = base->getReturnType();
		auto fnRet = fn->getReturnType();

		// ok now check the return type.
		return areTypesCovariant(baseRet, fnRet);
	}
}







