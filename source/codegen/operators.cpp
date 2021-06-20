// operators.cpp
// Copyright (c) 2014 - 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include "sst.h"
#include "codegen.h"

namespace cgn
{
	using OperatorFn = CodegenState::OperatorFn;
	std::pair<OperatorFn, fir::Function*> CodegenState::getOperatorFunctionForTypes(fir::Type* a, fir::Type* b, std::string op)
	{
		std::function<bool (fir::Type*)> isBuiltinType = [&](fir::Type* t) -> bool {
			if(t->isPrimitiveType())    return true;
			else if(t->isBoolType())    return true;
			else if(t->isCharType())    return true;
			else if(t->isNullType())    return true;
			else if(t->isVoidType())    return true;
			else if(t->isPointerType()) return true;
			else if(t->isArrayType() || t->isArraySliceType())
			{
				return isBuiltinType(t->getArrayElementType());
			}
			else if(t->isTupleType())
			{
				// uhm...
				bool res = false;
				for(auto e : t->toTupleType()->getElements())
					res &= isBuiltinType(e);

				return res;
			}
			else
			{
				return false;
			}
		};

		// check what the thing is
		if(isBuiltinType(a) && isBuiltinType(b))
		{
			return { OperatorFn::Builtin, 0 };
		}
		else
		{
			error(this->loc(), "unsupported operator function '%s' on types '%s' and '%s'", op, a, b);
		}
	}
}






