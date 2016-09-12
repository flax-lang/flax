// TypeAliasCodegen.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "ast.h"
#include "codegen.h"

using namespace Ast;
using namespace Codegen;

Result_t TypeAlias::codegen(CodegenInstance* cgi, fir::Value* extra)
{
	return Result_t(0, 0);
}

fir::Type* TypeAlias::getType(CodegenInstance* cgi, bool allowFail, fir::Value* extra)
{
	return 0;
}

fir::Type* TypeAlias::createType(CodegenInstance* cgi)
{
	iceAssert(0);
// 	this->ident.scope = cgi->getFullScope();

// 	fir::Type* targetType = 0;

// 	if(!this->isStrong)
// 	{
// 		targetType = cgi->getExprTypeFromStringType(this, this->origType);
// 	}
// 	else
// 	{
// 		targetType = fir::StructType::create(this->ident, { cgi->getExprTypeFromStringType(this, this->origType) });
// 		warn(this, "Strong type aliases are still iffy, use at your own risk");
// 	}

// 	cgi->addNewType(targetType, this, TypeKind::TypeAlias);
// 	return targetType;
}






















