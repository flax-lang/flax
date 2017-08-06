// literal.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "defs.h"
#include "parser_internal.h"

using namespace ast;
using namespace lexer;

using TT = TokenType;
namespace parser
{
	LitNumber* parseNumber(State& st)
	{
		iceAssert(st.front() == TT::Number);
		auto t = st.eat();

		return new LitNumber(st.ploc(), t.str());
	}
}
