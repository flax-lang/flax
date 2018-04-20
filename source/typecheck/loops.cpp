// loops.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "pts.h"
#include "ast.h"
#include "errors.h"
#include "typecheck.h"

#include "ir/type.h"

TCResult ast::ForeachLoop::typecheck(sst::TypecheckState* fs, fir::Type* inferred)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	auto ret = new sst::ForeachLoop(this->loc);
	auto n = fs->getAnonymousScopeName();

	fs->pushTree(n);
	defer(fs->popTree());


	ret->array = this->array->typecheck(fs).expr();

	fir::Type* elmty = 0;
	if(ret->array->type->isArrayType() || ret->array->type->isDynamicArrayType() || ret->array->type->isArraySliceType())
		elmty = ret->array->type->getArrayElementType();

	else if(ret->array->type->isRangeType())
		elmty = fir::Type::getInt64();

	else if(ret->array->type->isStringType())
		elmty = fir::Type::getChar();

	else
		error(this->array, "Invalid type '%s' in foreach loop", ret->array->type);

	iceAssert(elmty);
	bool allowref = !(ret->array->type->isStringType() || ret->array->type->isRangeType());

	if(!this->indexVar.empty())
	{
		auto fake = new ast::VarDefn(this->loc);
		fake->name = this->indexVar;
		fake->type = pts::NamedType::create(INT64_TYPE_STRING);

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

	sst::WhileLoop* ret = new sst::WhileLoop(this->loc);
	ret->isDoVariant = this->isDoVariant;

	auto n = fs->getAnonymousScopeName();

	fs->pushTree(n);
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
			error(this->cond, "Non-boolean expression with type '%s' cannot be used as a conditional", ret->cond->type);
	}

	return TCResult(ret);
}















TCResult ast::BreakStmt::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	if(!fs->isInBreakableBody())
		error(this, "Cannot 'break' while not inside a loop");

	else if(fs->isInDeferBlock())
		error(this, "Cannot 'break' while inside a deferred block");

	return TCResult(new sst::BreakStmt(this->loc));
}

TCResult ast::ContinueStmt::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	if(!fs->isInBreakableBody())
		error(this, "Cannot 'continue' while not inside a loop");

	else if(fs->isInDeferBlock())
		error(this, "Cannot 'continue' while inside a deferred block");

	return TCResult(new sst::ContinueStmt(this->loc));
}











