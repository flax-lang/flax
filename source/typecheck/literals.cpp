// literals.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "typecheck.h"

#include "ir/type.h"
#include "ir/constant.h"

TCResult ast::LitNumber::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	uint64_t raw = 0;
	size_t bits = 0;
	bool flt = 0;
	bool sgn = false;

	try
	{
		if(std::find(this->num.begin(), this->num.end(), '.') == this->num.end())
		{
			if(infer && infer->isPrimitiveType() && !infer->toPrimitiveType()->isSigned())
			{
				auto got = std::stoull(this->num, nullptr, 0);
				sgn = false;

				if(got <= UINT8_MAX)        bits = 8;
				else if(got <= UINT16_MAX)  bits = 16;
				else if(got <= UINT32_MAX)  bits = 32;
				else if(got <= UINT64_MAX)  bits = 64;
				else                        error("???");

				raw = (uint64_t) got;
			}
			else
			{
				auto got = std::stoll(this->num, nullptr, 0);
				sgn = (got < 0);

				if(got <= INT8_MAX && got >= INT8_MIN)          bits = 7;
				else if(got <= INT16_MAX && got >= INT16_MIN)   bits = 15;
				else if(got <= INT32_MAX && got >= INT32_MIN)   bits = 31;
				else if(got <= INT64_MAX && got >= INT64_MIN)   bits = 63;
				else                                            error("???");

				raw = (uint64_t) got;
			}
		}
		else
		{
			auto d = std::stod(this->num, nullptr);
			bits = 64;
			flt = true;
			raw = *reinterpret_cast<uint64_t*>(&d);
			sgn = (raw < 0);
		}
	}
	catch(const std::exception& e)
	{
		error("cannot do this thing");
	}

	auto ret = new sst::LiteralNumber(this->loc, (infer && infer->isPrimitiveType()) ? infer : fir::ConstantNumberType::get(sgn, flt, bits));
	ret->intgr = raw;

	return TCResult(ret);
}

TCResult ast::LitNull::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	auto ret = new sst::LiteralNull(this->loc, fir::Type::getNull());
	return TCResult(ret);
}

TCResult ast::LitBool::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	auto ret = new sst::LiteralBool(this->loc, fir::Type::getBool());
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
		auto expr = v->typecheck(fs, inf).expr();

		auto ty = expr->type;
		if(expr->type->isConstantNumberType() && (!inf || !inf->isPrimitiveType()))
			ty = fs->inferCorrectTypeForLiteral(expr->type->toConstantNumberType());

		vals.push_back(expr);
		fts.push_back(ty);

		k++;
	}

	// warn(this, "%s", fir::TupleType::get(fts));
	auto ret = new sst::LiteralTuple(this->loc, fir::TupleType::get(fts));
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

	auto ret = new sst::LiteralString(this->loc, ty);
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
					error(this, "Array type with non-zero length %zu was inferred for empty array literal", infer->toArrayType()->getArraySize());
			}
			else if(!(infer->isDynamicArrayType() || infer->isArraySliceType()))
			{
				error(this, "Invalid type '%s' inferred for array literal", infer);
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
				error(this, "Invalid type '%s' inferred for array literal", infer);

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
				error(v, "Mismatched type for expression in array literal; expected '%s'%s, found '%s'",
					elmty, (this->explicitType ? "" : " as inferred from previous elements"), e->type);
			}


			if(e->type->isVoidType())
			{
				// be helpful
				auto err = SimpleError::make(v, "Expected value in array literal, found 'void' value instead");

				if(auto fc = dcast(sst::FunctionCall, e); fc && fc->target)
					err.append(SimpleError(fc->target->loc, "Function was defined here:", MsgType::Note));

				err.postAndQuit();
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
			error(this, "Invalid type '%s' inferred for array literal", infer);
		}
	}

	auto ret = new sst::LiteralArray(this->loc, type);
	ret->values = vals;

	return TCResult(ret);
}














