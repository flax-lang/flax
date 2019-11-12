// directives.cpp
// Copyright (c) 2019, zhiayang
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "pts.h"
#include "errors.h"
#include "resolver.h"

#include "codegen.h"
#include "typecheck.h"

#include "ir/type.h"
#include "ir/interp.h"

#include "memorypool.h"


TCResult ast::RunDirective::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	auto rundir = util::pool<sst::RunDirective>(this->loc, nullptr);
	if(this->insideExpr)
	{
		rundir->insideExpr = this->insideExpr->typecheck(fs, infer).expr();
		rundir->type = rundir->insideExpr->type;
	}
	else
	{
		rundir->block = dcast(sst::Block, this->block->typecheck(fs, infer).stmt());
		iceAssert(rundir->block);

		rundir->type = fir::Type::getVoid();
	}

	return TCResult(rundir);
}




// defined in codegen/directives.cpp
fir::ConstantValue* magicallyRunExpressionAtCompileTime(cgn::CodegenState* cs, sst::Stmt* stmt, fir::Type* infer,
	const Identifier& fname, fir::interp::InterpState* is = 0);

static size_t condCounter = 0;
TCResult ast::IfDirective::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	// the entire point of #if is that when the condition is false, we don't typecheck it at all.
	// (of course, it must still parse.) in order to achieve this, together with arbitrary code in the
	// conditions, we first need to codegen anything that might have been seen -- eg. globals, functions, etc,
	// so that we can run the interpreter.

	// thus, we need to *run codegen* *right now*, to get a module. we don't call cgn::codegen directly, but we setup the
	// internals ourselves, since cgn::codegen expects a fully typechecked module/dtree, which we don't have right now
	auto mod = new fir::Module("");
	auto cs = new cgn::CodegenState(fir::IRBuilder(mod));
	cs->typeDefnMap = fs->typeDefnMap;
	cs->module = mod;

	// so we don't crash, give us a starting location.
	cs->pushLoc(this->loc);

	defer(delete cs);
	defer(delete mod);

	sst::Block* execBlock = 0;
	for(auto c : this->cases)
	{
		if(!c.inits.empty())
			error(c.inits[0], "compile-time #if currently does not support initialisers");

		auto cond = c.cond->typecheck(fs, fir::Type::getBool()).expr();
		auto value = magicallyRunExpressionAtCompileTime(cs, cond, fir::Type::getBool(),
			util::obfuscateIdentifier("interp_if_cond", condCounter++));

		if(!value->getType()->isBoolType())
			error(c.cond, "expression with non-boolean type '%s' cannot be used as a conditional in an #if", value->getType());

		auto b = dcast(fir::ConstantBool, value);
		iceAssert(b);

		if(b->getValue() == true)   // be a bit explicit
		{
			execBlock = dcast(sst::Block, c.body->typecheck(fs).stmt());
			iceAssert(execBlock);
			break;
		}
	}

	if(!execBlock && this->elseCase)
	{
		execBlock = dcast(sst::Block, this->elseCase->typecheck(fs).stmt());
		iceAssert(execBlock);
	}

	if(!execBlock)
	{
		return TCResult::getDummy();
	}
	else
	{
		return TCResult(execBlock);
	}
}































