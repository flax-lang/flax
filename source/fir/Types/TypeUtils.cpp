// TypeUtils.cpp
// Copyright (c) 2019, zhiayang
// Licensed under the Apache License Version 2.0.

#include "errors.h"
#include "ir/type.h"
#include "ir/function.h"

namespace fir
{
	bool areTypesCovariant(Type* base, Type* derv)
	{
		return false;
	}

	bool areTypesContravariant(Type* base, Type* derv, bool traitChecking)
	{
		return false;
	}

	bool areTypeListsContravariant(const std::vector<Type*>& base, const std::vector<Type*>& derv, bool traitChecking)
	{
		// parameters must be contravariant, ie. fn must take more general types than base
		// return type must be covariant, ie. fn must return a more specific type than base.

		// duh
		if(base.size() != derv.size())
			return false;

		// drop the first argument.
		for(auto [ b, d ] : zfu::zip(base, derv))
		{
			if(!areTypesContravariant(b, d, traitChecking))
				return false;
		}

		return true;
	}

	bool areMethodsVirtuallyCompatible(FunctionType* base, FunctionType* fn, bool traitChecking)
	{
		bool ret = areTypeListsContravariant(zfu::drop(base->getArgumentTypes(), 1),
			zfu::drop(fn->getArgumentTypes(), 1), traitChecking);

		if(!ret)
			return false;

		auto baseRet = base->getReturnType();
		auto fnRet = fn->getReturnType();

		// ok now check the return type.
		return areTypesCovariant(baseRet, fnRet);
	}
}







