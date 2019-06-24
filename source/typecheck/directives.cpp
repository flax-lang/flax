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

	auto expr = this->inside->typecheck(fs, infer).expr();
	auto rundir = util::pool<sst::RunDirective>(this->loc, expr->type);
	rundir->inside = expr;

	return TCResult(rundir);
}















