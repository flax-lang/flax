// directives.cpp
// Copyright (c) 2019, zhiayang
// Licensed under the Apache License Version 2.0.

#include "sst.h"
#include "codegen.h"
#include "platform.h"

#include "ir/interp.h"


fir::ConstantValue* magicallyRunExpressionAtCompileTime(cgn::CodegenState* cs, sst::Stmt* stmt, fir::Type* infer, const Identifier& fname)
{
	// what we do is to make a new function in IR, set the insertpoint to that,
	// then run codegen on the expression (so it generates inside), restore the insertpoint,
	// then run the interpreter on that function (after compiling it), then get the interp::Value
	// result, make a constantvalue with it


	auto restore = cs->irb.getCurrentBlock();

	bool isExpr = false;
	fir::Type* retty = 0;
	if(auto ex = dcast(sst::Expr, stmt); ex)    isExpr = true, retty = ex->type;
	else                                        retty = fir::Type::getVoid();

	auto fn = cs->module->getOrCreateFunction(fname, fir::FunctionType::get({ }, retty), fir::LinkageType::Internal);
	iceAssert(fn);

	// make the function:
	{
		auto entry = cs->irb.addNewBlockInFunction("entry", fn);
		 cs->irb.setCurrentBlock(entry);

		fir::Value* ret = 0;

		if(isExpr)  ret = stmt->codegen(cs, infer).value;
		else        stmt->codegen(cs, infer);

		if(!ret || ret->getType()->isVoidType())
			cs->irb.ReturnVoid();

		else
			cs->irb.Return(ret);

		if(restore) cs->irb.setCurrentBlock(restore);
	}

	// run the function:
	fir::ConstantValue* ret = 0;
	{
		auto is = new fir::interp::InterpState(cs->module);
		is->initialise();
		{
			auto result = is->runFunction(is->compileFunction(fn), { });

			if(!retty->isVoidType())
				ret = is->unwrapInterpValueIntoConstant(result);
		}
		is->finalise();

		delete is;
	}

	return ret;
}







static size_t runDirectiveId = 0;
CGResult sst::RunDirective::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	sst::Stmt* toExec = 0;
	if(this->insideExpr)    toExec = this->insideExpr;
	else                    toExec = this->block;

	auto ret = magicallyRunExpressionAtCompileTime(cs, toExec, infer, util::obfuscateIdentifier("run_directive", runDirectiveId++));
	return CGResult(ret);
}
































