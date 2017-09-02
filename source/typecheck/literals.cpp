// literals.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "errors.h"
#include "typecheck.h"

#include "ir/type.h"

using TCS = sst::TypecheckState;

#define dcast(t, v)		dynamic_cast<t*>(v)

sst::Stmt* ast::LitNumber::typecheck(TCS* fs, fir::Type* infer)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	if(this->num.find('.') != std::string::npos)
	{
		// decimal
		auto ret = new sst::LiteralDec(this->loc);
		ret->number = std::stold(this->num);
		ret->type = fir::PrimitiveType::getConstantFloat();

		return ret;
	}
	else
	{
		auto ret = new sst::LiteralInt(this->loc);
		if(this->num.find('-') != std::string::npos)
		{
			ret->negative = true;
		 	ret->number = std::stoll(this->num);
		 	ret->type = fir::PrimitiveType::getConstantSignedInt();
		}
		else
		{
			ret->negative = false;

			try
			{
				ret->number = std::stoll(this->num);
				ret->type = fir::PrimitiveType::getConstantSignedInt();
			}
			catch(std::out_of_range& e)
			{
				ret->number = std::stoull(this->num);
				ret->type = fir::PrimitiveType::getConstantUnsignedInt();
			}
		}

		return ret;
	}
}

sst::Stmt* ast::LitNull::typecheck(TCS* fs, fir::Type* infer)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	auto ret = new sst::LiteralNull(this->loc);
	ret->type = fir::Type::getNull();

	return ret;
}

sst::Stmt* ast::LitBool::typecheck(TCS* fs, fir::Type* infer)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	auto ret = new sst::LiteralBool(this->loc);
	ret->value = this->value;
	ret->type = fir::Type::getBool();

	return ret;
}

sst::Stmt* ast::LitTuple::typecheck(TCS* fs, fir::Type* infer)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	auto ret = new sst::LiteralTuple(this->loc);

	std::vector<fir::Type*> fts;
	for(auto v : this->values)
	{
		auto val = v->typecheck(fs);
		sst::Expr* expr = dcast(sst::Expr, val);
		if(!expr)
			expected(v->loc, "expression", "statement");

		ret->values.push_back(expr);
		fts.push_back(expr->type);
	}

	ret->type = fir::TupleType::get(fts);
	return ret;
}

sst::Stmt* ast::LitString::typecheck(TCS* fs, fir::Type* infer)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	auto ret = new sst::LiteralString(this->loc);

	ret->str = this->str;
	ret->isCString = this->isCString;

	if(this->isCString || (infer && infer == fir::Type::getInt8Ptr()))
	{
		ret->type = fir::Type::getInt8Ptr();
	}
	else
	{
		ret->type = fir::Type::getStringType();
	}

	return ret;
}
















