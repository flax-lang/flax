// loops.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "errors.h"
#include "typecheck.h"

#include "ir/type.h"

#define dcast(t, v)		dynamic_cast<t*>(v)


sst::Stmt* ast::ForeachLoop::typecheck(sst::TypecheckState* fs, fir::Type* inferred)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	auto ret = new sst::ForeachLoop(this->loc);

	auto n = fs->getAnonymousScopeName();

	fs->pushTree(n);
	defer(fs->popTree());


	// check the array
	ret->array = this->array->typecheck(fs);

	fir::Type* elmty = 0;
	if(ret->array->type->isArrayType() || ret->array->type->isDynamicArrayType() || ret->array->type->isArraySliceType())
		elmty = ret->array->type->getArrayElementType();

	else if(ret->array->type->isRangeType())
		elmty = fir::Type::getInt64();

	else if(ret->array->type->isStringType())
		elmty = fir::Type::getChar();

	else
		error(this->array, "Invalid type '%s' in foreach loop", ret->array->type);

	if(this->var != "_")
	{
		auto fake = new sst::VarDefn(this->varloc);
		fake->id = Identifier(this->var, IdKind::Name);
		fake->id.scope = fs->getCurrentScope();

		{
			fs->stree->addDefinition(this->var, fake);
		}

		fake->type = elmty;
		fake->immutable = true;
		ret->var = fake;
	}
	else
	{
		ret->var = 0;
	}


	fs->enterBreakableBody();
	{
		ret->body = dcast(sst::Block, this->body->typecheck(fs));
		iceAssert(ret->body);
	}
	fs->leaveBreakableBody();

	return ret;
}


























sst::Stmt* ast::WhileLoop::typecheck(sst::TypecheckState* fs, fir::Type* inferred)
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
		ret->body = dcast(sst::Block, this->body->typecheck(fs));
		iceAssert(ret->body);
	}
	fs->leaveBreakableBody();

	if(this->cond)
	{
		ret->cond = this->cond->typecheck(fs, fir::Type::getBool());
		if(ret->cond->type != fir::Type::getBool() && !ret->cond->type->isPointerType())
			error(this->cond, "Non-boolean expression with type '%s' cannot be used as a conditional", ret->cond->type);
	}

	return ret;
}















sst::Stmt* ast::BreakStmt::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	if(!fs->isInBreakableBody())
		error(this, "Cannot 'break' while not inside a loop");

	else if(fs->isInDeferBlock())
		error(this, "Cannot 'break' while inside a deferred block");

	return new sst::BreakStmt(this->loc);
}

sst::Stmt* ast::ContinueStmt::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	if(!fs->isInBreakableBody())
		error(this, "Cannot 'continue' while not inside a loop");

	else if(fs->isInDeferBlock())
		error(this, "Cannot 'continue' while inside a deferred block");

	return new sst::ContinueStmt(this->loc);
}
