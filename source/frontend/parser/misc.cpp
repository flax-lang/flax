// misc.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "frontend.h"
#include "parser_internal.h"

using namespace ast;
using namespace lexer;

namespace parser
{
	ImportStmt* parseImport(State& st)
	{
		using TT = lexer::TokenType;
		iceAssert(st.eat() == TT::Import);

		if(st.frontAfterWS() == TT::StringLiteral)
		{
			auto ret = new ImportStmt(st.loc(), st.frontAfterWS().str());
			ret->resolvedModule = frontend::resolveImport(ret->path, ret->loc, st.currentFilePath);

			st.eat();
			return ret;
		}
		else
		{
			expected(st, "string literal after 'import' for module specifier", st.frontAfterWS().str());
		}
	}
}

void expected(const Location& loc, std::string a, std::string b)
{
	error(loc, "Expected %s, found '%s' instead", a, b);
}

void expectedAfter(const Location& loc, std::string a, std::string b, std::string c)
{
	error(loc, "Expected %s after %s, found '%s' instead", a, b, c);
}

void unexpected(const Location& loc, std::string a)
{
	error(loc, "Unexpected %s", a);
}













