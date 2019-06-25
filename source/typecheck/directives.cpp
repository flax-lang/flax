// directives.cpp
// Copyright (c) 2019, zhiayang
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "pts.h"
#include "errors.h"

#include "ir/type.h"
#include "resolver.h"

#include "typecheck.h"

#include "mpool.h"


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















