// literals.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "typecheck.h"

#include "ir/type.h"
#include "ir/constant.h"

using TCS = sst::TypecheckState;

#define dcast(t, v)		dynamic_cast<t*>(v)

sst::Expr* ast::LitNumber::typecheck(TCS* fs, fir::Type* infer)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	if(this->num.front() == '-')
		error("i wish");

	auto n = mpfr::mpreal(this->num);
	auto ret = new sst::LiteralNumber(this->loc, (infer && infer->isPrimitiveType()) ? infer : fir::Type::getConstantNumber(n));
	ret->number = n;

	return ret;
}

sst::Expr* ast::LitNull::typecheck(TCS* fs, fir::Type* infer)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	auto ret = new sst::LiteralNull(this->loc, fir::Type::getNull());
	return ret;
}

sst::Expr* ast::LitBool::typecheck(TCS* fs, fir::Type* infer)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	auto ret = new sst::LiteralBool(this->loc, fir::Type::getBool());
	ret->value = this->value;

	return ret;
}

sst::Expr* ast::LitTuple::typecheck(TCS* fs, fir::Type* infer)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	std::vector<sst::Expr*> vals;
	std::vector<fir::Type*> fts;

	if(infer)
	{
		if(!infer->isTupleType())
			error(this, "Assigning tuple to inferred non-tuple type '%s'", infer);

		auto tt = infer->toTupleType();
		if(tt->getElementCount() != this->values.size())
		{
			error(this, "Mismatched types in inferred type: have literal with %zu elements, inferred type has %zu", this->values.size(),
				tt->getElementCount());
		}
	}

	size_t k = 0;
	for(auto v : this->values)
	{
		auto inf = (infer ? infer->toTupleType()->getElementN(k) : 0);
		auto expr = v->typecheck(fs, inf);

		auto ty = expr->type;
		if(expr->type->isConstantNumberType() && (!inf || !inf->isPrimitiveType()))
			ty = fs->inferCorrectTypeForLiteral(expr);

		vals.push_back(expr);
		fts.push_back(ty);

		k++;
	}

	// warn(this, "%s", fir::TupleType::get(fts));
	auto ret = new sst::LiteralTuple(this->loc, fir::TupleType::get(fts));
	ret->values = vals;

	return ret;
}

sst::Expr* ast::LitString::typecheck(TCS* fs, fir::Type* infer)
{
	fs->pushLoc(this);
	defer(fs->popLoc());


	fir::Type* ty = 0;
	if(this->isCString || (infer && infer == fir::Type::getInt8Ptr()))
	{
		ty = fir::Type::getInt8Ptr();
	}
	else
	{
		ty = fir::Type::getString();
	}

	auto ret = new sst::LiteralString(this->loc, ty);
	ret->isCString = this->isCString;
	ret->str = this->str;

	return ret;
}

sst::Expr* ast::LitArray::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	std::vector<sst::Expr*> vals;

	fir::Type* type = 0;
	if(this->values.empty())
	{
		if(infer == 0)
		{
			// facilitate passing empty array literals around (that can be cast to a bunch of things like slices and such)
			infer = fir::DynamicArrayType::get(fir::VoidType::get());
		}
			// error(this, "Unable to infer type for empty array literal");

		// okay.
		if(infer->isArrayType())
		{
			if(infer->toArrayType()->getArraySize() != 0)
				error(this, "Array type with non-zero length %zu was inferred for empty array literal", infer->toArrayType()->getArraySize());
		}
		else if(!(infer->isDynamicArrayType() || infer->isArraySliceType()))
		{
			error(this, "Invalid type '%s' inferred for array literal", infer);
		}

		type = infer;
	}
	else
	{
		fir::Type* elmty = 0;

		if(infer)
		{
			if(!infer->isDynamicArrayType() && !infer->isArraySliceType() && !infer->isArrayType())
				error(this, "Invalid type '%s' inferred for array literal", infer);

			elmty = infer->getArrayElementType();
		}

		for(auto v : this->values)
		{
			auto e = v->typecheck(fs, elmty);

			if(!elmty)
			{
				if(e->type->isConstantNumberType() && !infer)
					elmty = fs->inferCorrectTypeForLiteral(e);

				else
					elmty = e->type;
			}
			else if(elmty != e->type)
			{
				error(v, "Mismatched type for expression in array literal; expected '%s' as inferred from previous elements, found '%s'",
					elmty, e->type);
			}


			if(e->type->isVoidType())
			{
				// be helpful
				exitless_error(v, "Expected value in array literal, found 'void' value instead");
				if(auto fc = dcast(sst::FunctionCall, e); fc && fc->target)
					info(fc->target, "Function was defined here:");

				doTheExit();
			}

			vals.push_back(e);
		}

		// note: try and prefer dynamic arrays.
		if(this->raw || (infer && infer->isArrayType()))
		{
			type = fir::ArrayType::get(elmty, this->values.size());
		}
		else if(infer == 0 || infer->isDynamicArrayType())
		{
			// do something
			type = fir::DynamicArrayType::get(elmty);
		}
		else if(infer->isArraySliceType())
		{
			// do something
			type = fir::ArraySliceType::get(elmty);
		}
		else
		{
			error(this, "Invalid type '%s' inferred for array literal", infer);
		}
	}

	auto ret = new sst::LiteralArray(this->loc, type);
	ret->values = vals;

	return ret;
}














