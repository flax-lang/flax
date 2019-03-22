// literals.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "typecheck.h"

#include "ir/type.h"
#include "ir/constant.h"

#include "mpool.h"

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
	bool flt = !mpfr::isint(number);

	//* this is the stupidest thing.

	// mpfr's 'get_min_prec' returns the number of bits required to store the significand (eg. for 1.413x10^-2, it is 1.413).
	// so you'd think that, for example, given '1024', it would return '10', given that 2^10 == 1024.
	// no, it returns '1', because you only need one bit -- the 10th bit -- to get the value 1024.
	// which is fucking stupid.

	// so what we do here is we change the last digit of the number to be '9'. this effectively forces the first
	// bit of the entire number to be set (we don't use '1' because we don't want to make the number smaller
	// -- eg. 1024 would become 1021, which would only need 9 bits to store -- versus 1029)

	// in this way we force mpfr to return the real number of bits required to store the entire thing properly.

	size_t bits = mpfr_min_prec(mpfr::mpreal(this->num.substr(0, this->num.size() - 1) + "9").mpfr_ptr());

	printf("number %s: %d / %d\n", number.toString().c_str(), sgn, bits);

	printf("bits for %s: %d\n", this->num.c_str(), mpfr_min_prec(mpfr::mpreal(this->num).mpfr_ptr()));

	auto ret = util::pool<sst::LiteralNumber>(this->loc, (infer && infer->isPrimitiveType()) ? infer : fir::ConstantNumberType::get(sgn, flt, bits));
	ret->num = mpfr::mpreal(this->num);

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














