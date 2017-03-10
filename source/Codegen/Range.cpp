// Range.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "codegen.h"

using namespace Ast;
using namespace Codegen;

Result_t Range::codegen(CodegenInstance* cgi, fir::Type* extratype, fir::Value* target)
{
	fir::Value* lowerbd = this->start->codegen(cgi).value;
	fir::Value* upperbd = this->end->codegen(cgi).value;

	if(!lowerbd->getType()->isIntegerType())
		error(this->start, "Lower bound of range is not of an integer type (got '%s')", lowerbd->getType()->str().c_str());

	if(!upperbd->getType()->isIntegerType())
		error(this->start, "Upper bound of range is not of an integer type (got '%s')", upperbd->getType()->str().c_str());


	lowerbd = cgi->irb.CreateIntSizeCast(lowerbd, fir::Type::getInt64());
	upperbd = cgi->irb.CreateIntSizeCast(upperbd, fir::Type::getInt64());

	// ok. now create that shit.
	if(this->isHalfOpen)
		upperbd = cgi->irb.CreateSub(upperbd, fir::ConstantInt::getInt64(1));

	fir::Value* ret = cgi->irb.CreateValue(fir::Type::getRangeType());
	ret = cgi->irb.CreateSetRangeLower(ret, lowerbd);
	ret = cgi->irb.CreateSetRangeUpper(ret, upperbd);

	return Result_t(ret, 0);
}

fir::Type* Range::getType(CodegenInstance* cgi, fir::Type* extratype, bool allowFail)
{
	return fir::Type::getRangeType();
}
