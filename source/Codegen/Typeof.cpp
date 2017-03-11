// TypeofCodegen.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "codegen.h"

using namespace Ast;
using namespace Codegen;


Result_t Typeof::codegen(CodegenInstance* cgi, fir::Type* extratype, fir::Value* target)
{
	iceAssert(0);
}

fir::Type* Typeof::getType(CodegenInstance* cgi, fir::Type* extratype, bool allowFail)
{
	iceAssert(0);
}








Result_t Typeid::codegen(CodegenInstance* cgi, fir::Type* extratype, fir::Value* target)
{
	// first check for types
	if(auto vr = dynamic_cast<VarRef*>(this->inside))
	{
		if(fir::Type* bt = cgi->getExprTypeOfBuiltin(vr->name))
		{
			return Result_t(fir::ConstantInt::getInt64(bt->getID()), 0);
		}
		else if(TypePair_t* tp = cgi->getTypeByString(vr->name))
		{
			// use the type
			fir::Type* t = tp->first;
			if(!t) t = tp->second.first->getType(cgi);

			return Result_t(fir::ConstantInt::getInt64(t->getID()), 0);
		}
	}

	fir::Value* val = 0; fir::Value* ptr = 0;
	std::tie(val, ptr) = this->inside->codegen(cgi);

	if(val->getType()->isAnyType())
	{
		if(!ptr)
			ptr = cgi->irb.CreateImmutStackAlloc(val->getType(), val);

		iceAssert(ptr);

		return Result_t(cgi->irb.CreateGetAnyTypeID(ptr), 0);
	}
	else
	{
		return Result_t(fir::ConstantInt::getInt64(val->getType()->getID()), 0);
	}
}

fir::Type* Typeid::getType(CodegenInstance* cgi, fir::Type* extratype, bool allowFail)
{
	return fir::Type::getInt64();
}




