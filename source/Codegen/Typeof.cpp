// TypeofCodegen.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "codegen.h"

using namespace Ast;
using namespace Codegen;


// static Result_t getTypeOfAny(CodegenInstance* cgi, fir::Value* ptr)
// {
// 	fir::Value* gep = cgi->irb.CreateStructGEP(ptr, 0);
// 	return Result_t(cgi->irb.CreateLoad(gep), gep);
// }

Result_t Typeof::codegen(CodegenInstance* cgi, fir::Value* extra)
{
	error(this, "go away");

	#if 0
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
			type = decl->getType(cgi);
		}
	}
	else
	{
		type = this->inside->getType(cgi);
	}


	size_t index = 0;
	if(cgi->isAnyType(type))
	{
		auto r = this->inside->codegen(cgi);
		return getTypeOfAny(cgi, r.pointer);
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
	fir::Value* gep = cgi->irb.CreateStructGEP(wrapper, 0);

	cgi->irb.CreateStore(enr->cases[index - 1].second->codegen(cgi).value, gep);
	return Result_t(cgi->irb.CreateLoad(wrapper), wrapper);
	#endif
}

fir::Type* Typeof::getType(CodegenInstance* cgi, bool allowFail, fir::Value* extra)
{
	#if 0
	TypePair_t* tp = cgi->getTypeByString("Type");
	iceAssert(tp);

	return tp->first;
	#endif

	iceAssert(0);
	return 0;
}










