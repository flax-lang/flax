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


	// check if upper > lower
	fir::Value* cond = cgi->irb.CreateICmpGEQ(upperbd, lowerbd);
	auto merge = cgi->irb.addNewBlockInFunction("merge", cgi->irb.getCurrentFunction());
	auto fail = cgi->irb.addNewBlockInFunction("fail", cgi->irb.getCurrentFunction());

	cgi->irb.CreateCondBranch(cond, merge, fail);

	cgi->irb.setCurrentBlock(fail);
	{
		fir::Function* fprintfn = cgi->module->getOrCreateFunction(Identifier("fprintf", IdKind::Name),
			fir::FunctionType::getCVariadicFunc({ fir::Type::getVoidPtr(), fir::Type::getInt8Ptr() },
			fir::Type::getInt32()), fir::LinkageType::External);

		fir::Function* fdopenf = cgi->module->getOrCreateFunction(Identifier("fdopen", IdKind::Name),
			fir::FunctionType::get({ fir::Type::getInt32(), fir::Type::getInt8Ptr() }, fir::Type::getVoidPtr(), false),
			fir::LinkageType::External);

		// basically:
		// void* stderr = fdopen(2, "w")
		// fprintf(stderr, "", bla bla)

		fir::ConstantValue* tmpstr = cgi->module->createGlobalString("w");
		fir::ConstantValue* fmtstr = cgi->module->createGlobalString("%s: Upper bound of range is less than lower bound (l: %zd, u: %zd)\n");

		iceAssert(fmtstr);

		fir::Value* posstr = cgi->irb.CreateGetStringData(cgi->makeStringLiteral(Parser::pinToString(this->pin)).value);
		fir::Value* err = cgi->irb.CreateCall2(fdopenf, fir::ConstantInt::getInt32(2), tmpstr);

		cgi->irb.CreateCall(fprintfn, { err, fmtstr, posstr, lowerbd, upperbd });

		cgi->irb.CreateCall0(cgi->getOrDeclareLibCFunc("abort"));
		cgi->irb.CreateUnreachable();
	}


	cgi->irb.setCurrentBlock(merge);

	fir::Value* ret = cgi->irb.CreateValue(fir::Type::getRangeType());
	ret = cgi->irb.CreateSetRangeLower(ret, lowerbd);
	ret = cgi->irb.CreateSetRangeUpper(ret, upperbd);

	return Result_t(ret, 0);
}

fir::Type* Range::getType(CodegenInstance* cgi, fir::Type* extratype, bool allowFail)
{
	return fir::Type::getRangeType();
}
