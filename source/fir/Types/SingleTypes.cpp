// SingleTypes.cpp
// Copyright (c) 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ir/type.h"

namespace fir
{
	static AnyType* singleAny = 0;
	AnyType::AnyType()                          { }
	std::string AnyType::str()                  { return "any"; }
	std::string AnyType::encodedStr()           { return "any"; }
	bool AnyType::isTypeEqual(Type* other)      { return other && other->isAnyType(); }
	AnyType* AnyType::get()                     { return singleAny = (singleAny ? singleAny : new AnyType()); }


	static BoolType* singleBool = 0;
	BoolType::BoolType()                        { }
	std::string BoolType::str()                 { return "bool"; }
	std::string BoolType::encodedStr()          { return "bool"; }
	bool BoolType::isTypeEqual(Type* other)     { return other && other->isBoolType(); }
	BoolType* BoolType::get()                   { return singleBool = (singleBool ? singleBool : new BoolType()); }


	static VoidType* singleVoid = 0;
	VoidType::VoidType()                        { }
	std::string VoidType::str()                 { return "void"; }
	std::string VoidType::encodedStr()          { return "void"; }
	bool VoidType::isTypeEqual(Type* other)     { return other && other->isVoidType(); }
	VoidType* VoidType::get()                   { return singleVoid = (singleVoid ? singleVoid : new VoidType()); }


	static NullType* singleNull = 0;
	NullType::NullType()                        { }
	std::string NullType::str()                 { return "nulltype"; }
	std::string NullType::encodedStr()          { return "nulltype"; }
	bool NullType::isTypeEqual(Type* other)     { return other && other->isNullType(); }
	NullType* NullType::get()                   { return singleNull = (singleNull ? singleNull : new NullType()); }


	static RangeType* singleRange = 0;
	RangeType::RangeType()                      { }
	std::string RangeType::str()                { return "range"; }
	std::string RangeType::encodedStr()         { return "range"; }
	bool RangeType::isTypeEqual(Type* other)    { return other && other->isRangeType(); }
	RangeType* RangeType::get()                 { return singleRange = (singleRange ? singleRange : new RangeType()); }


	static StringType* singleString = 0;
	StringType::StringType()                    { }
	std::string StringType::str()               { return "string"; }
	std::string StringType::encodedStr()        { return "string"; }
	bool StringType::isTypeEqual(Type* other)   { return other && other->isStringType(); }
	StringType* StringType::get()               { return singleString = (singleString ? singleString : new StringType()); }



	std::string ConstantNumberType::str()                       { return "number"; }
	std::string ConstantNumberType::encodedStr()                { return "number"; }
	ConstantNumberType::ConstantNumberType(mpfr::mpreal n)      { this->number = n; }
	mpfr::mpreal ConstantNumberType::getValue()                 { return this->number; }
	ConstantNumberType* ConstantNumberType::get(mpfr::mpreal n) { return new ConstantNumberType(n); }
	bool ConstantNumberType::isTypeEqual(Type* other)           { return other && other->isConstantNumberType(); }
}






















