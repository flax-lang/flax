// directives.cpp
// Copyright (c) 2019, zhiayang
// Licensed under the Apache License Version 2.0.

#include <memory>

#include "sst.h"
#include "codegen.h"
#include "platform.h"

#include "ir/interp.h"

#include "memorypool.h"

fir::ConstantValue* magicallyRunExpressionAtCompileTime(cgn::CodegenState* cs, sst::Stmt* stmt, fir::Type* infer,
	const Identifier& fname, fir::interp::InterpState* is = 0)
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
	iceAssert(stmt);

	// make the function:
	{
		auto entry = cs->irb.addNewBlockInFunction("entry", fn);
		cs->irb.setCurrentBlock(entry);

		// we need a block, even though we don't codegen -- this is to handle raii/refcounting things.
		// stack allocate, so we can get rid of it later. (automatically)
		auto fakeBlk = sst::Block(Location());
		cs->enterBlock(&fakeBlk);

		fir::Value* ret = 0;

		if(isExpr)  ret = stmt->codegen(cs, infer).value;
		else        stmt->codegen(cs, infer);

		if(!ret || ret->getType()->isVoidType())
			cs->irb.ReturnVoid();

		else
			cs->irb.Return(ret);


		cs->leaveBlock();

		if(restore) cs->irb.setCurrentBlock(restore);
	}

	// finalise the global init function if necessary:
	cs->finishGlobalInitFunction();

	// run the function:
	fir::ConstantValue* ret = 0;
	{
		// this unique_ptr handles destructing the temporary interpState when we're done.
		using unique_ptr_alias = std::unique_ptr<fir::interp::InterpState, std::function<void (fir::interp::InterpState*)>>;
		auto ptr = unique_ptr_alias();
		if(!is)
		{
			is = new fir::interp::InterpState(cs->module);
			is->initialise(/* runGlobalInit: */ true);
			ptr = unique_ptr_alias(is, [](fir::interp::InterpState* is) {
				is->finalise();
				delete is;
			});
		}
		else
		{
			// new strategy: run the initialisers anyway.
			is->initialise(/* runGlobalInit: */ true);

			// caller code will finalise.
		}

		auto result = is->runFunction(is->compileFunction(fn), { });

		if(!retty->isVoidType())
			ret = is->unwrapInterpValueIntoConstant(result);
	}

	// please get rid of the runner function
	cs->module->removeFunction(fn);
	delete fn;


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
































