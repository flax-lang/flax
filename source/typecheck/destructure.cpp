// destructure.cpp
// Copyright (c) 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "errors.h"
#include "typecheck.h"

#include "ir/type.h"

static void checkAndAddBinding(sst::TypecheckState* fs, DecompMapping* bind, fir::Type* rhs, bool immut, bool allowref);
static void checkTuple(sst::TypecheckState* fs, DecompMapping* bind, fir::Type* rhs, bool immut)
{
	iceAssert(!bind->array);
	if(!rhs->isTupleType())
		error(bind->loc, "Expected tuple type in destructuring declaration; found type '%s' instead", rhs);

	auto tty = rhs->toTupleType();
	if(bind->inner.size() != tty->getElementCount())
	{
		error(bind->loc, "Too %s bindings in destructuring declaration; expected %d, found %d instead",
			(bind->inner.size() < tty->getElementCount() ? "few" : "many"), tty->getElementCount(), bind->inner.size());
	}

	for(size_t i = 0; i < tty->getElementCount(); i++)
		checkAndAddBinding(fs, &bind->inner[i], tty->getElementN(i), immut, true);
}

static void checkArray(sst::TypecheckState* fs, DecompMapping* bind, fir::Type* rhs, bool immut)
{
	iceAssert(bind->array);

	if(!rhs->isArrayType() && !rhs->isDynamicArrayType() && !rhs->isArraySliceType() && !rhs->isStringType())
		error(bind->loc, "Expected array type in destructuring declaration; found type '%s' instead", rhs);

	if(rhs->isStringType())
	{
		//* note: special-case this, because 1. we want to return chars, but 2. strings are supposed to be immutable.
		for(auto& b : bind->inner)
			checkAndAddBinding(fs, &b, fir::Type::getInt8(), immut, false);

		if(!bind->restName.empty())
		{
			auto fake = new sst::VarDefn(bind->loc);

			fake->id = Identifier(bind->restName, IdKind::Name);
			fake->immutable = immut;

			//* note: see typecheck/slices.cpp for mutability rules.
			if(bind->restRef)   fake->type = fir::Type::getCharSlice(sst::getMutabilityOfSliceOfType(rhs));
			else                fake->type = fir::Type::getString();

			fs->stree->addDefinition(bind->restName, fake);

			bind->restDefn = fake;
		}
	}
	else
	{
		for(auto& b : bind->inner)
			checkAndAddBinding(fs, &b, rhs->getArrayElementType(), immut, true);

		if(!bind->restName.empty())
		{
			auto fake = new sst::VarDefn(bind->loc);

			fake->id = Identifier(bind->restName, IdKind::Name);
			fake->immutable = immut;

			//* note: see typecheck/slices.cpp for mutability rules.
			if(bind->restRef || rhs->isArraySliceType())
				fake->type = fir::ArraySliceType::get(rhs->getArrayElementType(), sst::getMutabilityOfSliceOfType(rhs));

			else
				fake->type = fir::DynamicArrayType::get(rhs->getArrayElementType());

			fs->stree->addDefinition(bind->restName, fake);
			bind->restDefn = fake;
		}
	}
}



static void checkAndAddBinding(sst::TypecheckState* fs, DecompMapping* bind, fir::Type* rhs, bool immut, bool allowref)
{
	if(!bind->name.empty())
	{
		if(bind->name != "_")
		{
			auto fake = new sst::VarDefn(bind->loc);

			pushErrorLocation(fake);
			defer(popErrorLocation());

			fake->id = Identifier(bind->name, IdKind::Name);
			fake->immutable = immut;

			if(bind->ref && !allowref)
				error(bind->loc, "Cannot bind to value of type '%s' by reference", rhs);

			else if(bind->ref)
				fake->type = rhs->getPointerTo();

			else
				fake->type = rhs;

			fs->stree->addDefinition(bind->name, fake);
			bind->createdDefn = fake;
		}
	}
	else if(bind->array)
	{
		checkArray(fs, bind, rhs, immut);
	}
	else
	{
		checkTuple(fs, bind, rhs, immut);
	}
}


DecompMapping sst::TypecheckState::typecheckDecompositions(const DecompMapping& bind, fir::Type* rhs, bool immut, bool allowref)
{
	auto copy = bind;
	checkAndAddBinding(this, &copy, rhs, immut, allowref);

	return copy;
}


TCResult ast::DecompVarDefn::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	fs->pushLoc(this);
	defer(fs->popLoc());


	auto ret = new sst::DecompDefn(this->loc);

	ret->immutable = this->immut;
	if(auto splat = dcast(ast::SplatOp, this->initialiser))
	{
		if(this->bindings.array)
		{
			HighlightOptions ho; ho.caret = this->loc; ho.underlines.push_back(this->initialiser->loc);
			error(this, ho, "Value splats can only be assigned to tuple decompositions");
		}

		bool isnest = false;
		for(const auto& b : this->bindings.inner)
		{
			if(b.name.empty())
			{
				isnest = true;
				break;
			}
		}

		if(isnest)
		{
			HighlightOptions ho; ho.caret = this->loc; ho.underlines.push_back(this->initialiser->loc);
			error(this, ho, "Value splats can only be assigned to single-level tuple decompositions; nesting is not allowed.");
		}

		// ok, at this point we should be fine.
		this->initialiser = new ast::LitTuple(splat->loc, std::vector<ast::Expr*>(this->bindings.inner.size(), splat->expr));
	}

	ret->init = this->initialiser->typecheck(fs).expr();
	ret->bindings = fs->typecheckDecompositions(this->bindings, ret->init->type, this->immut, false);

	return TCResult(ret);
}












