// TypeAliasCodegen.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "../include/ast.h"
#include "../include/codegen.h"
#include "../include/llvm_all.h"

using namespace Ast;
using namespace Codegen;

Result_t TypeAlias::codegen(CodegenInstance* cgi, llvm::Value* lhsPtr, llvm::Value* rhs)
{
	llvm::Type* targetType = cgi->unwrapPointerType(this->origType);

	if(!this->isStrong)
	{
		Struct* dummy = new Struct(this->posinfo, this->alias);
		cgi->addNewType(targetType, dummy, ExprType::TypeAlias);

		return Result_t(0, 0);
	}
	else
	{
		/*
		to implement:
		do an enum job -- aka wrap the target type in a struct
		then only unwrap it at the last moment, thus making sure that mangling and
		type resolution uses the aliased name strongly, not accepting the original name

		code refactor: group this code with the enum code, for a lastMinuteUnwrapType() kind of call
		currently binop and vardecl both check for isEnum() and manually unwrap the type

		note:
		function calls don't need this kind of special treatment, since the typename used for
		mangling is indeed the aliased type (or enum type) and the FuncCall::codegen() will report
		undeclared function when calling with the base type
		*/


		error(this, "Strong type aliases are not supported yet");
	}
}
