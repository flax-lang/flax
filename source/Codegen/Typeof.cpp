// TypeofCodegen.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "../include/ast.h"
#include "../include/codegen.h"

using namespace Ast;
using namespace Codegen;

Result_t Typeof::codegen(CodegenInstance* cgi, fir::Value* lhsPtr, fir::Value* rhs)
{
	size_t index = 0;
	if(VarRef* vr = dynamic_cast<VarRef*>(this->inside))
	{
		VarDecl* decl = cgi->getSymDecl(this, vr->name);
		fir::Type* t = 0;
		if(!decl)
		{
			t = cgi->getLlvmTypeFromExprType(this, vr->name);

			if(!t)
				GenError::unknownSymbol(cgi, vr, vr->name, SymbolType::Variable);
		}
		else
		{
			t = cgi->getLlvmType(decl);
		}

		index = TypeInfo::getIndexForType(cgi, t);
	}
	else
	{
		fir::Type* t = cgi->getLlvmType(this->inside);
		index = TypeInfo::getIndexForType(cgi, t);
	}


	if(index == 0)
		error(this, "invalid shit!");


	TypePair_t* tp = cgi->getType("Type");
	iceAssert(tp);

	Enumeration* enr = dynamic_cast<Enumeration*>(tp->second.first);
	iceAssert(enr);

	fir::Value* wrapper = cgi->allocateInstanceInBlock(tp->first, "typeof_tmp");
	fir::Value* gep = cgi->builder.CreateStructGEP(wrapper, 0, "wrapped");

	cgi->builder.CreateStore(enr->cases[index - 1].second->codegen(cgi).result.first, gep);
	return Result_t(cgi->builder.CreateLoad(wrapper), wrapper);
}










