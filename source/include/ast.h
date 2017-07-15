// ast.h
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once
#include "defs.h"

namespace ast
{
	struct Expr
	{
		Expr(const Location& l) : loc(l) { }
		virtual ~Expr();

		Location loc;
	};

	struct Ident : Expr
	{
		Ident(const Location& l, std::string n);

		std::string name;
	};
}
