// alloc.cpp
// Copyright (c) 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "errors.h"
#include "typecheck.h"
#include "pts.h"

#include "ir/type.h"

sst::Expr* ast::AllocOp::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	fir::Type* elm = fs->convertParserTypeToFIR(this->allocTy);
	iceAssert(elm);

	if(this->isRaw && this->counts.size() > 1)
		error(this, "Only one length dimension is supported for raw memory allocation (have %d)", this->counts.size());

	std::vector<sst::Expr*> counts = util::map(this->counts, [fs](ast::Expr* e) -> auto {
		auto c = e->typecheck(fs, fir::Type::getInt64());
		if(!c->type->isIntegerType())
			error(c, "Expected integer type ('i64') for alloc count, found '%s' instead", c->type);

		return c;
	});

	// check for initialiser.
	if(!elm->isClassType() && !elm->isStructType() && this->args.size() > 0)
		error(this, "Cannot provide arguments to non-struct type '%s'", elm);


	fir::Type* resType = (this->isRaw || counts.empty() ?
		elm->getPointerTo() : fir::DynamicArrayType::get(elm));

	auto ret = new sst::AllocOp(this->loc, resType);


	// ok, check if we're a struct.
	if((elm->isStructType() || elm->isClassType()))
	{
		auto cdf = fs->typeDefnMap[elm];
		iceAssert(cdf);

		using Param = sst::FunctionDecl::Param;
		sst::TypecheckState::PrettyError errs;

		auto arguments = fs->typecheckCallArguments(this->args);
		auto constructor = fs->resolveConstructorCall(cdf, util::map(arguments, [](FnCallArgument a) -> Param {
			return Param { a.name, a.loc, a.value->type };
		}), &errs);

		if(!constructor)
		{
			exitless_error(this, "%s", errs.errorStr);
			for(auto i : errs.infoStrs)
				info(i.first, "%s", i.second);

			doTheExit();
		}

		ret->constructor = constructor;
		ret->arguments = arguments;
	}
	else if(!this->args.empty())
	{
		if(this->args.size() > 1) error(this, "Expected 1 argument in alloc expression for non-struct type '%s' (for value-copy-initialisation), but found %d arguments instead", this->args.size());

		auto args = fs->typecheckCallArguments(this->args);
		if(args[0].value->type != elm)
			error(this, "Expected argument of type '%s' for value-copy-initialisation in alloc expression, but found '%s' instead", elm, args[0].value->type);

		// ok loh
		ret->arguments = args;
	}

	if(this->initBody)
	{
		iceAssert(!this->isRaw && this->counts.size() > 0);

		// ok, make a fake vardefn and insert it first.
		auto fake = new ast::VarDefn(this->initBody->loc);
		fake->type = this->allocTy;
		fake->name = "it";

		auto fake2 = new ast::VarDefn(this->initBody->loc);
		fake2->type = pts::NamedType::create(INT64_TYPE_STRING);
		fake2->name = "i";

		// make a temp scope to enclose it, I guess
		fs->pushTree(fs->getAnonymousScopeName());
		{
			ret->initBlockVar = dcast(sst::VarDefn, fake->typecheck(fs));
			ret->initBlockIdx = dcast(sst::VarDefn, fake2->typecheck(fs));
			ret->initBlock = dcast(sst::Block, this->initBody->typecheck(fs));

			iceAssert(ret->initBlockVar && ret->initBlockIdx && ret->initBlock);
		}
		fs->popTree();
	}


	ret->elmType = elm;
	ret->counts = counts;
	ret->isRaw = this->isRaw;

	return ret;
}

sst::Stmt* ast::DeallocOp::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	auto ex = this->expr->typecheck(fs);
	if(ex->type->isDynamicArrayType())
		error(ex, "Dynamic arrays are reference-counted, and cannot be manually freed");

	else if(!ex->type->isPointerType())
		error(ex, "Expected pointer or dynamic array type to deallocate; found '%s' instead", ex->type);

	auto ret = new sst::DeallocOp(this->loc);
	ret->expr = ex;

	return ret;
}







