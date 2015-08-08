// TypeofCodegen.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "../include/ast.h"
#include "../include/codegen.h"
#include "../include/llvm_all.h"

using namespace Ast;
using namespace Codegen;

Result_t Typeof::codegen(CodegenInstance* cgi, llvm::Value* lhsPtr, llvm::Value* rhs)
{
	size_t index = 0;
	if(VarRef* vr = dynamic_cast<VarRef*>(this->inside))
	{
		VarDecl* decl = cgi->getSymDecl(this, vr->name);
		llvm::Type* t = 0;
		if(!decl)
		{
			t = cgi->parseTypeFromString(this, vr->name);

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
		llvm::Type* t = cgi->getLlvmType(this->inside);
		index = TypeInfo::getIndexForType(cgi, t);
	}


	if(index == 0)
		error(cgi, this, "invalid shit!");


	TypePair_t* tp = cgi->getType("Type");
	iceAssert(tp);

	Enumeration* enr = dynamic_cast<Enumeration*>(tp->second.first);
	iceAssert(enr);

	llvm::Value* wrapper = cgi->allocateInstanceInBlock(tp->first, "typeof_tmp");
	llvm::Value* gep = cgi->builder.CreateStructGEP(wrapper, 0, "wrapped");

	cgi->builder.CreateStore(enr->cases[index - 1].second->codegen(cgi).result.first, gep);
	return Result_t(cgi->builder.CreateLoad(wrapper), wrapper);
}










