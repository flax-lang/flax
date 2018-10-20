// misc.cpp
// Copyright (c) 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "errors.h"
#include "typecheck.h"

#include "ir/type.h"
#include "mpool.h"

TCResult ast::TypeExpr::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	auto ret = sst::TypeExpr::make(this->loc, fs->convertParserTypeToFIR(this->type));
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
	fs->pushLoc(this);
	defer(fs->popLoc());

	auto inside = this->expr->typecheck(fs, infer).expr();

	if(!inside->type->isArraySliceType() && !inside->type->isArrayType() && !inside->type->isDynamicArrayType() && !inside->type->isTupleType())
		return TCResult(SimpleError::make(this->loc, "invalid use of splat operator on type '%s'", inside->type));

	if(inside->type->isTupleType())
		return TCResult(SimpleError::make(this->loc, "splat operator on tuple not allowed in this context"));

	auto ret = util::pool<sst::SplatExpr>(this->loc, fir::ArraySliceType::getVariadic(inside->type->getArrayElementType()));
	ret->inside = inside;

	return TCResult(ret);
}

TCResult ast::Parameterisable::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	return this->typecheck(fs, infer, { });
}


static std::unordered_map<fir::Type*, sst::TypeExpr*> cache;
sst::TypeExpr* sst::TypeExpr::make(const Location& l, fir::Type* t)
{
	if(auto it = cache.find(t); it != cache.end())
		return it->second;

	return (cache[t] = util::pool<sst::TypeExpr>(l, t));
}

FnCallArgument FnCallArgument::make(const Location& l, const std::string& n, fir::Type* t)
{
	auto te = sst::TypeExpr::make(l, t);
	return FnCallArgument(l, n, te, nullptr);
}
















