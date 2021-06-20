// loops.cpp
// Copyright (c) 2014 - 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include "pts.h"
#include "ast.h"
#include "errors.h"
#include "typecheck.h"

#include "ir/type.h"

#include "memorypool.h"

TCResult ast::ForeachLoop::typecheck(sst::TypecheckState* fs, fir::Type* inferred)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	auto ret = util::pool<sst::ForeachLoop>(this->loc);

	fs->pushAnonymousTree();
	defer(fs->popTree());


	ret->array = this->array->typecheck(fs).expr();

	fir::Type* elmty = 0;
	if(ret->array->type->isArrayType() || ret->array->type->isArraySliceType())
		elmty = ret->array->type->getArrayElementType();

	else if(ret->array->type->isRangeType())
		elmty = fir::Type::getNativeWord();

	else
		error(this->array, "invalid type '%s' in foreach loop", ret->array->type);

	iceAssert(elmty);
	bool allowref = !ret->array->type->isRangeType();

	if(!this->indexVar.empty())
	{
		auto fake = util::pool<ast::VarDefn>(this->loc);
		fake->name = this->indexVar;
		fake->type = pts::NamedType::create(this->loc, INTUNSPEC_TYPE_STRING);

		ret->indexVar = dcast(sst::VarDefn, fake->typecheck(fs).defn());
		iceAssert(ret->indexVar);
	}

	ret->mappings = fs->typecheckDecompositions(this->bindings, elmty, true, allowref);

	fs->enterBreakableBody();
	{
		ret->body = dcast(sst::Block, this->body->typecheck(fs).stmt());
		iceAssert(ret->body);
	}
	fs->leaveBreakableBody();

	return TCResult(ret);
}

























TCResult ast::WhileLoop::typecheck(sst::TypecheckState* fs, fir::Type* inferred)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	sst::WhileLoop* ret = util::pool<sst::WhileLoop>(this->loc);
	ret->isDoVariant = this->isDoVariant;

	fs->pushAnonymousTree();
	defer(fs->popTree());

	fs->enterBreakableBody();
	{
		ret->body = dcast(sst::Block, this->body->typecheck(fs).stmt());
		iceAssert(ret->body);
	}
	fs->leaveBreakableBody();

	if(this->cond)
	{
		ret->cond = this->cond->typecheck(fs, fir::Type::getBool()).expr();
		if(ret->cond->type != fir::Type::getBool() && !ret->cond->type->isPointerType())
			error(this->cond, "non-boolean expression with type '%s' cannot be used as a conditional", ret->cond->type);
	}

	return TCResult(ret);
}















TCResult ast::BreakStmt::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	if(!fs->isInBreakableBody())
		error(this, "cannot 'break' while not inside a loop");

	else if(fs->isInDeferBlock())
		error(this, "cannot 'break' while inside a deferred block");

	return TCResult(util::pool<sst::BreakStmt>(this->loc));
}

TCResult ast::ContinueStmt::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	if(!fs->isInBreakableBody())
		error(this, "cannot 'continue' while not inside a loop");

	else if(fs->isInDeferBlock())
		error(this, "cannot 'continue' while inside a deferred block");

	return TCResult(util::pool<sst::ContinueStmt>(this->loc));
}











