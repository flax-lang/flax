// literals.cpp
// Copyright (c) 2014 - 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "typecheck.h"

#include "ir/type.h"
#include "ir/constant.h"

#include "memorypool.h"

TCResult ast::LitNumber::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	// i don't think mpfr auto-detects base, LMAO
	int base = 10;
	if(this->num.find("0x") == 0 || this->num.find("0X") == 0)
		base = 16;

	auto number = mpfr::mpreal(this->num, mpfr_get_default_prec(), base);
	bool sgn = mpfr::signbit(number);
	bool flt = ((this->num.find(".") != std::string::npos) || !mpfr::isint(number));

	size_t bits = 0;
	if(flt)
	{
		// fuck it lah.
		bits = sizeof(double) * CHAR_BIT;
	}
	else
	{
		auto m_ptr = number.mpfr_ptr();
		auto m_rnd = MPFR_RNDN;
		if(mpfr_fits_sshort_p(m_ptr, m_rnd))
			bits = sizeof(short) * CHAR_BIT;

		else if(mpfr_fits_sint_p(m_ptr, m_rnd))
			bits = sizeof(int) * CHAR_BIT;

		else if(mpfr_fits_slong_p(m_ptr, m_rnd))
			bits = sizeof(long) * CHAR_BIT;

		else if(mpfr_fits_intmax_p(m_ptr, m_rnd))
			bits = sizeof(intmax_t) * CHAR_BIT;

		else if(!sgn && mpfr_fits_uintmax_p(m_ptr, m_rnd))
			bits = sizeof(uintmax_t) * CHAR_BIT;

		else    // lmao
			bits = sizeof(uintmax_t) * CHAR_BIT;
	}

	auto ret = util::pool<sst::LiteralNumber>(this->loc, (infer && infer->isPrimitiveType()) ? infer
		: fir::ConstantNumberType::get(sgn, flt, bits));

	ret->num = number;

	return TCResult(ret);
}

TCResult ast::LitNull::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	auto ret = util::pool<sst::LiteralNull>(this->loc, fir::Type::getNull());
	return TCResult(ret);
}

TCResult ast::LitBool::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	auto ret = util::pool<sst::LiteralBool>(this->loc, fir::Type::getBool());
	ret->value = this->value;

	return TCResult(ret);
}

TCResult ast::LitChar::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	auto ret = util::pool<sst::LiteralChar>(this->loc, fir::Type::getInt8());
	ret->value = this->value;

	return TCResult(ret);
}

TCResult ast::LitTuple::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	std::vector<sst::Expr*> vals;
	std::vector<fir::Type*> fts;

	if(infer)
	{
		if(!infer->isTupleType())
			error(this, "assigning tuple to inferred non-tuple type '%s'", infer);

		auto tt = infer->toTupleType();
		if(tt->getElementCount() != this->values.size())
		{
			error(this, "mismatched types in inferred type: have literal with %zu elements, inferred type has %zu", this->values.size(),
				tt->getElementCount());
		}
	}

	size_t k = 0;
	for(auto v : this->values)
	{
		auto inf = (infer ? infer->toTupleType()->getElementN(k) : 0);
		auto expr = v->typecheck(fs, inf).expr();

		auto ty = expr->type;
		if(expr->type->isConstantNumberType() && (!inf || !inf->isPrimitiveType()))
			ty = fs->inferCorrectTypeForLiteral(expr->type->toConstantNumberType());

		vals.push_back(expr);
		fts.push_back(ty);

		k++;
	}

	// warn(this, "%s", fir::TupleType::get(fts));
	auto ret = util::pool<sst::LiteralTuple>(this->loc, fir::TupleType::get(fts));
	ret->values = vals;

	return TCResult(ret);
}

TCResult ast::LitString::typecheck(sst::TypecheckState* fs, fir::Type* infer)
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
		ty = fir::Type::getCharSlice(false);
	}

	auto ret = util::pool<sst::LiteralString>(this->loc, ty);
	ret->isCString = this->isCString;
	ret->str = this->str;

	return TCResult(ret);
}

TCResult ast::LitArray::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	std::vector<sst::Expr*> vals;

	fir::Type* type = 0;
	if(this->values.empty())
	{
		if(this->explicitType)
		{
			auto explty = fs->convertParserTypeToFIR(this->explicitType);
			iceAssert(explty);

			type = fir::DynamicArrayType::get(explty);
		}
		else
		{
			if(infer == 0)
			{
				// facilitate passing empty array literals around (that can be cast to a bunch of things like slices and such)
				infer = fir::DynamicArrayType::get(fir::VoidType::get());
			}
			else if(infer->isArrayType())
			{
				if(infer->toArrayType()->getArraySize() != 0)
					error(this, "array type with non-zero length %zu was inferred for empty array literal", infer->toArrayType()->getArraySize());
			}
			else if(!(infer->isDynamicArrayType() || infer->isArraySliceType()))
			{
				error(this, "invalid type '%s' inferred for array literal", infer);
			}
		}

		type = infer;
	}
	else
	{
		fir::Type* elmty = (this->explicitType ? fs->convertParserTypeToFIR(this->explicitType) : 0);

		if(!elmty && infer)
		{
			if(!infer->isDynamicArrayType() && !infer->isArraySliceType() && !infer->isArrayType())
				error(this, "invalid type '%s' inferred for array literal", infer);

			elmty = infer->getArrayElementType();
		}

		for(auto v : this->values)
		{
			auto e = v->typecheck(fs, elmty).expr();

			if(!elmty)
			{
				if(e->type->isConstantNumberType() && !infer)
					elmty = fs->inferCorrectTypeForLiteral(e->type->toConstantNumberType());

				else
					elmty = e->type;
			}
			else if(elmty != e->type)
			{
				error(v, "mismatched type for expression in array literal; expected '%s'%s, found '%s'",
					elmty, (this->explicitType ? "" : " as inferred from previous elements"), e->type);
			}


			if(e->type->isVoidType())
			{
				// be helpful
				auto err = SimpleError::make(v->loc, "expected value in array literal, found 'void' value instead");

				if(auto fc = dcast(sst::FunctionCall, e); fc && fc->target)
					err->append(SimpleError::make(MsgType::Note, fc->target->loc, "function was defined here:"));

				err->postAndQuit();
			}

			vals.push_back(e);
		}

		//* note: prefer slices by default.
		// this behaviour changed as of 08/04/2018
		if(this->raw || (infer && infer->isArrayType()))
		{
			type = fir::ArrayType::get(elmty, this->values.size());
		}
		else if(infer == 0 || infer->isArraySliceType())
		{
			// slices from a constant array generally should remain immutable.
			type = fir::ArraySliceType::get(elmty, false);
		}
		else if(infer->isDynamicArrayType())
		{
			// do something
			type = fir::DynamicArrayType::get(elmty);
		}
		else
		{
			error(this, "invalid type '%s' inferred for array literal", infer);
		}
	}

	auto ret = util::pool<sst::LiteralArray>(this->loc, type);
	ret->values = vals;

	return TCResult(ret);
}














