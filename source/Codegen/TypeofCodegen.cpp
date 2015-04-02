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
		if(!decl)
			GenError::unknownSymbol(cgi, vr, vr->name, SymbolType::Variable);

		llvm::Type* t = cgi->getLlvmType(decl);
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
	assert(tp);

	Enumeration* enr = dynamic_cast<Enumeration*>(tp->second.first);
	assert(enr);

	llvm::Value* wrapper = cgi->allocateInstanceInBlock(tp->first, "tmp_shit");
	llvm::Value* gep = cgi->mainBuilder.CreateStructGEP(wrapper, 0, "wrapped");

	cgi->mainBuilder.CreateStore(enr->cases[index - 1].second->codegen(cgi).result.first, gep);
	return Result_t(cgi->mainBuilder.CreateLoad(wrapper), wrapper);
}










