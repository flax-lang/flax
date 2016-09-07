// TypeofCodegen.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "codegen.h"

using namespace Ast;
using namespace Codegen;


static Result_t getTypeOfAny(CodegenInstance* cgi, fir::Value* ptr)
{
	fir::Value* gep = cgi->builder.CreateStructGEP(ptr, 0);
	return Result_t(cgi->builder.CreateLoad(gep), gep);
}

Result_t Typeof::codegen(CodegenInstance* cgi, fir::Value* extra)
{
	size_t index = 0;
	fir::Type* type = 0;

	if(VarRef* vr = dynamic_cast<VarRef*>(this->inside))
	{
		VarDecl* decl = cgi->getSymDecl(this, vr->name);
		if(!decl)
		{
			type = cgi->getTypeFromParserType(this, pts::NamedType::create(vr->name));

			if(!type)
				GenError::unknownSymbol(cgi, vr, vr->name, SymbolType::Variable);
		}
		else
		{
			type = cgi->getExprType(decl);
		}
	}
	else
	{
		type = cgi->getExprType(this->inside);
	}


	if(cgi->isAnyType(type))
	{
		ValPtr_t vp = this->inside->codegen(cgi).result;
		fir::Value* ptr = vp.second;

		return getTypeOfAny(cgi, ptr);
	}
	else
	{
		index = TypeInfo::getIndexForType(cgi, type);
	}




	if(index == 0)
		error(this, "invalid shit!");


	TypePair_t* tp = cgi->getTypeByString("Type");
	iceAssert(tp);

	EnumDef* enr = dynamic_cast<EnumDef*>(tp->second.first);
	iceAssert(enr);

	fir::Value* wrapper = cgi->getStackAlloc(tp->first, "typeof_tmp");
	fir::Value* gep = cgi->builder.CreateStructGEP(wrapper, 0);

	cgi->builder.CreateStore(enr->cases[index - 1].second->codegen(cgi).result.first, gep);
	return Result_t(cgi->builder.CreateLoad(wrapper), wrapper);
}










