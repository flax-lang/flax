// stcommon.h
// Copyright (c) 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#pragma once
#include "defs.h"

namespace sst
{
	struct VarDefn;
}

namespace ast
{
	struct Expr;
}

struct DecompMapping
{
	Location loc;
	std::string name;
	bool ref = false;
	bool array = false;

	sst::VarDefn* createdDefn = 0;

	std::vector<DecompMapping> inner;

	// for array decompositions, this will hold the rest.
	std::string restName;
	bool restRef = false;
	sst::VarDefn* restDefn = 0;
};

struct FnCallArgument
{
	FnCallArgument() { }
	FnCallArgument(const Location& l, const std::string& n, sst::Expr* v, ast::Expr* o) : loc(l), name(n), value(v), orig(o) { }

	Location loc;
	std::string name;
	sst::Expr* value = 0;

	ast::Expr* orig = 0;

	static FnCallArgument make(const Location& l, const std::string& n, fir::Type* t);

	bool operator == (const FnCallArgument& other) const
	{
		return this->loc == other.loc && this->name == other.name && this->value == other.value && this->orig == other.orig;
	}
};

struct FnParam
{
	FnParam() { }
	FnParam(const Location& l, const std::string& n, fir::Type* t) : loc(l), name(n), type(t) { }

	Location loc;
	std::string name;
	fir::Type* type = 0;
	sst::Expr* defaultVal = 0;
};




















