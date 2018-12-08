// misc.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "frontend.h"
#include "parser_internal.h"

#include "mpool.h"

using namespace ast;
using namespace lexer;

namespace parser
{
	using TT = lexer::TokenType;
	ImportStmt* parseImport(State& st)
	{
		iceAssert(st.front() == TT::Import);
		auto ret = util::pool<ImportStmt>(st.loc());

		st.eat();
		st.skipWS();

		if(st.front() == TT::StringLiteral)
		{
			st.eat();
		}
		else if(st.front() == TT::Identifier)
		{
			// just consume.
			size_t i = st.getIndex();
			parseIdentPath(st.getTokenList(), &i);
			st.setIndex(i);
		}
		else
		{
			expectedAfter(st, "string literal or identifier path", "'import'", st.front().str());
		}


		{
			st.skipWS();

			// check for 'import as foo'
			if(st.front() == TT::As)
			{
				st.eat();
				if(st.front() != TT::Identifier)
					expectedAfter(st.loc(), "identifier", "'import-as'", st.front().str());

				size_t i = st.getIndex();
				parseIdentPath(st.getTokenList(), &i);
				st.setIndex(i);
			}

			return ret;
		}
	}

	UsingStmt* parseUsingStmt(State& st)
	{
		iceAssert(st.front() == TT::Using);
		st.eat();

		st.enterUsingParse();
		defer(st.leaveUsingParse());

		auto ret = util::pool<UsingStmt>(st.ploc());
		ret->expr = parseExpr(st);

		if(st.front() != TT::As)
			expectedAfter(st.loc(), "'as'", "scope in 'using'", st.front().str());

		st.eat();
		if(st.front() != TT::Identifier)
			expectedAfter(st.loc(), "identifier", "'as' in 'using' declaration", st.front().str());

		ret->useAs = st.eat().str();
		return ret;
	}
}

void expected(const Location& loc, std::string a, std::string b)
{
	error(loc, "expected %s, found '%s' instead", a, b);
}

void expectedAfter(const Location& loc, std::string a, std::string b, std::string c)
{
	error(loc, "expected %s after %s, found '%s' instead", a, b, c);
}

void unexpected(const Location& loc, std::string a)
{
	error(loc, "unexpected %s", a);
}













