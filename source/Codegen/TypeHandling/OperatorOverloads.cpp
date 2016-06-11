// OperatorOverloads.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "ast.h"
#include "codegen.h"

using namespace Ast;
using namespace Codegen;



Result_t SubscriptOpOverload::codegen(Codegen::CodegenInstance *cgi, fir::Value* extra)
{
	return Result_t(0, 0);
}


Result_t AssignOpOverload::codegen(Codegen::CodegenInstance *cgi, fir::Value* extra)
{
	return Result_t(0, 0);
}




Result_t OpOverload::codegen(CodegenInstance* cgi, fir::Value* extra)
{
	if(!this->didCodegen)
	{
		this->didCodegen = true;

		auto res = this->func->codegen(cgi);
		this->lfunc = dynamic_cast<fir::Function*>(res.result.first);

		return res;
	}
	else
	{
		iceAssert(this->lfunc);
		return Result_t(this->lfunc, 0);
	}
}









































