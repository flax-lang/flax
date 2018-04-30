// sizeof.cpp
// Copyright (c) 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "pts.h"
#include "errors.h"
#include "ir/type.h"
#include "typecheck.h"

TCResult ast::SizeofOp::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	auto ret = new sst::SizeofOp(this->loc, fir::Type::getInt64());

	// see what we have.
	fir::Type* out = 0;
	if(auto id = dcast(ast::Ident, this->expr))
	{
		if(auto ty = fs->convertParserTypeToFIR(pts::NamedType::create(id->name), true))
			out = ty;
	}
	else if(dcast(ast::LitNumber, this->expr))
	{
		error(this->expr, "Literal numbers cannot be sized");
	}

	if(!out) out = this->expr->typecheck(fs).expr()->type;

	iceAssert(out);
	ret->typeToSize = out;

	return TCResult(ret);
}