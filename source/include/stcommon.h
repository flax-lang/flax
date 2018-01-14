// stcommon.h
// Copyright (c) 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once
#include "defs.h"

namespace sst
{
	struct VarDefn;
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
	FnCallArgument(const Location& l, const std::string& n, sst::Expr* v) : loc(l), name(n), value(v) { }

	Location loc;
	std::string name;

	sst::Expr* value = 0;
};