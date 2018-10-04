// misc.cpp
// Copyright (c) 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "errors.h"
#include "typecheck.h"

TCResult ast::TypeExpr::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	auto ret = new sst::TypeExpr(this->loc, fs->convertParserTypeToFIR(this->type));
	return TCResult(ret);
}

TCResult ast::MutabilityTypeExpr::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	error(this, "Unable to typecheck mutability cast, this shouldn't happen!");
}

TCResult ast::ImportStmt::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	// nothing to check??
	unexpected(this->loc, "import statement");
}

TCResult ast::SplatOp::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	// error(this, "Unable to typecheck splat op, this shouldn't happen!");
	return this->expr->typecheck(fs, infer);
}

TCResult ast::Parameterisable::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	return this->typecheck(fs, infer, { });
}
