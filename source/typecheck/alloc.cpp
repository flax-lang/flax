// alloc.cpp
// Copyright (c) 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "pts.h"
#include "errors.h"
#include "typecheck.h"

#include "ir/type.h"
#include "resolver.h"

#include "memorypool.h"

TCResult ast::AllocOp::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	fir::Type* elm = fs->convertParserTypeToFIR(this->allocTy);
	iceAssert(elm);

	if(this->attrs.has(attr::RAW) && this->counts.size() > 1)
		error(this, "only one length dimension is supported for raw memory allocation (have %d)", this->counts.size());

	std::vector<sst::Expr*> counts = zfu::map(this->counts, [fs](ast::Expr* e) -> auto {
		auto c = e->typecheck(fs, fir::Type::getNativeWord()).expr();
		if(!c->type->isIntegerType())
			error(c, "expected integer type ('i64') for alloc count, found '%s' instead", c->type);

		return c;
	});

	// check for initialiser.
	if(!elm->isClassType() && !elm->isStructType() && this->args.size() > 0)
		error(this, "cannot provide arguments to non-struct type '%s'", elm);


	fir::Type* resType = (this->attrs.has(attr::RAW) || counts.empty() ?
		(this->isMutable ? elm->getMutablePointerTo() : elm->getPointerTo()) : fir::DynamicArrayType::get(elm));

	auto ret = util::pool<sst::AllocOp>(this->loc, resType);


	// ok, check if we're a struct.
	if((elm->isStructType() || elm->isClassType()))
	{
		auto cdf = fs->typeDefnMap[elm];
		iceAssert(cdf);

		auto arguments = sst::resolver::misc::typecheckCallArguments(fs, this->args);
		auto constructor = sst::resolver::resolveConstructorCall(fs, this->loc, cdf, arguments, PolyArgMapping_t::none());

		ret->constructor = constructor.defn();
		ret->arguments = arguments;
	}
	else if(!this->args.empty())
	{
		if(this->args.size() > 1) error(this, "expected 1 argument in alloc expression for non-struct type '%s' (for value-copy-initialisation), but found %d arguments instead", this->args.size());

		auto args = sst::resolver::misc::typecheckCallArguments(fs, this->args);
		if(args[0].value->type != elm)
			error(this, "expected argument of type '%s' for value-copy-initialisation in alloc expression, but found '%s' instead", elm, args[0].value->type);

		// ok loh
		ret->arguments = args;
	}

	if(this->initBody)
	{
		iceAssert(!this->attrs.has(attr::RAW) && this->counts.size() > 0);

		// ok, make a fake vardefn and insert it first.
		auto fake = util::pool<ast::VarDefn>(this->initBody->loc);
		fake->type = this->allocTy;
		fake->name = "it";

		auto fake2 = util::pool<ast::VarDefn>(this->initBody->loc);
		fake2->type = pts::NamedType::create(this->initBody->loc, INTUNSPEC_TYPE_STRING);
		fake2->name = "i";

		// make a temp scope to enclose it, I guess
		fs->pushAnonymousTree();
		{
			ret->initBlockVar = dcast(sst::VarDefn, fake->typecheck(fs).defn());
			ret->initBlockIdx = dcast(sst::VarDefn, fake2->typecheck(fs).defn());
			ret->initBlock = dcast(sst::Block, this->initBody->typecheck(fs).stmt());

			iceAssert(ret->initBlockVar && ret->initBlockIdx && ret->initBlock);
		}
		fs->popTree();
	}


	ret->elmType    = elm;
	ret->counts     = counts;
	ret->attrs      = this->attrs;
	ret->isMutable  = this->isMutable;

	return TCResult(ret);
}

TCResult ast::DeallocOp::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	auto ex = this->expr->typecheck(fs).expr();
	if(ex->type->isDynamicArrayType())
		error(ex, "dynamic arrays are reference-counted, and cannot be manually freed");

	else if(!ex->type->isPointerType())
		error(ex, "expected pointer or dynamic array type to deallocate; found '%s' instead", ex->type);

	auto ret = util::pool<sst::DeallocOp>(this->loc);
	ret->expr = ex;

	return TCResult(ret);
}







