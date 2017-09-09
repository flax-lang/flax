// literals.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "errors.h"
#include "typecheck.h"

#include "ir/type.h"
#include "ir/constant.h"

using TCS = sst::TypecheckState;

#define dcast(t, v)		dynamic_cast<t*>(v)

sst::Expr* ast::LitNumber::typecheck(TCS* fs, fir::Type* infer)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	auto n = mpfr::mpreal(this->num);
	auto ret = new sst::LiteralNumber(this->loc, infer ? infer : fir::Type::getConstantNumber(n));
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
	for(auto v : this->values)
	{
		auto val = v->typecheck(fs);
		sst::Expr* expr = dcast(sst::Expr, val);
		if(!expr)
			expected(v->loc, "expression", "statement");

		vals.push_back(expr);
		fts.push_back(expr->type);
	}

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
		ty = fir::Type::getStringType();
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
			error(this, "Unable to infer type for empty array literal");

		// okay.
		if(infer->isArrayType())
		{
			if(infer->toArrayType()->getArraySize() != 0)
				error(this, "Array type with non-zero length %zu was inferred for empty array literal", infer->toArrayType()->getArraySize());
		}
		else if(!(infer->isDynamicArrayType() || infer->isArraySliceType()))
		{
			error(this, "Invalid type '%s' inferred for array literal", infer->str());
		}

		type = infer;
	}
	else
	{
		fir::Type* elmty = 0;

		if(infer)
		{
			if(infer->isDynamicArrayType())		elmty = infer->toDynamicArrayType()->getElementType();
			else if(infer->isArrayType())		elmty = infer->toArrayType()->getElementType();
			else if(infer->isArraySliceType())	elmty = infer->toArraySliceType()->getElementType();
			else								error(this, "Invalid type '%s' inferred for array literal", infer->str());
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
					elmty->str(), e->type->str());
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
			error(this, "Invalid type '%s' inferred for array literal", infer->str());
		}
	}

	auto ret = new sst::LiteralArray(this->loc, type);
	ret->values = vals;

	return ret;
}














