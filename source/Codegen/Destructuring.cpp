// Destructuring.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "codegen.h"

using namespace Codegen;

namespace Ast
{
	Result_t DestructuredTupleDecl::codegen(CodegenInstance* cgi, fir::Value* extra)
	{
		return Result_t(0, 0);
	}

	fir::Type* DestructuredTupleDecl::getType(CodegenInstance* cgi, bool allowFail, fir::Value* extra)
	{
		return 0;
	}
}
