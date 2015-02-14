// BuiltinTypes.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "../include/parser.h"
#include "../include/codegen.h"
#include "../include/llvm_all.h"

using namespace Ast;
using namespace Codegen;


namespace Codegen
{
	Result_t handleBuiltinTypeAccess(CodegenInstance* cgi, MemberAccess* ma)
	{
		assert(cgi);
		assert(ma);

		// // gen the var ref on the left.
		// ValPtr_t p = ma->target->codegen(cgi).result;

		// llvm::Value* self = p.first;
		// llvm::Value* selfPtr = p.second;

		// // how2builtin-string

		return Result_t(0, 0);
	}
}































