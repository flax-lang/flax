// directives.cpp
// Copyright (c) 2019, zhiayang
// Licensed under the Apache License Version 2.0.

#include "sst.h"
#include "codegen.h"
#include "platform.h"

#include "ir/interp.h"


static size_t runDirectiveId = 0;
CGResult sst::RunDirective::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	cs->pushLoc(this);
	defer(cs->popLoc());

	auto restore = cs->irb.getCurrentBlock();

	// what we do is to make a new function in IR, set the insertpoint to that,
	// then run codegen on the expression (so it generates inside), restore the insertpoint,
	// then run the interpreter on that function (after compiling it), then get the interp::Value
	// result, make a constantvalue with it

	fir::Type* retty = 0;
	if(this->insideExpr)    retty = this->insideExpr->type;
	else                    retty = fir::Type::getVoid();

	auto fname = util::obfuscateIdentifier("run_directive", runDirectiveId++);
	auto fn = cs->module->getOrCreateFunction(fname, fir::FunctionType::get({ }, retty), fir::LinkageType::Internal);
	iceAssert(fn);

	{
		auto entry = cs->irb.addNewBlockInFunction("entry", fn);
		cs->irb.setCurrentBlock(entry);

		fir::Value* ret = 0;
		if(this->insideExpr)    ret = this->insideExpr->codegen(cs, infer).value;
		else                    this->block->codegen(cs, infer);

		if(!ret || ret->getType()->isVoidType())
			cs->irb.ReturnVoid();

		else
			cs->irb.Return(ret);

		if(restore) cs->irb.setCurrentBlock(restore);
	}

	// ok, now we create an interpstate, and run the function.
	auto is = new fir::interp::InterpState(cs->module);
	is->initialise();

	fir::Value* ret = 0;
	{
		auto result = is->runFunction(is->compileFunction(fn), { });
		is->finalise();

		if(!retty->isVoidType())
			ret = is->unwrapInterpValueIntoConstant(result);
	}

	delete is;
	return CGResult(ret);
}
































