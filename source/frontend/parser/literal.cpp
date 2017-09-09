// literal.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "defs.h"
#include "parser_internal.h"

#include <sstream>

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

	LitString* parseString(State& st, bool israw)
	{
		iceAssert(st.front() == TT::StringLiteral);
		auto t = st.eat();

		// do replacement here, instead of in the lexer.
		std::string tmp = t.str();
		std::stringstream ss;

		for(size_t i = 0; i < tmp.length(); i++)
		{
			if(tmp[i] == '\\')
			{
				i++;
				switch(tmp[i])
				{
					// todo: handle hex sequences and stuff
					case 'n':	ss << '\n';	break;
					case 'b':	ss << '\b';	break;
					case 'r':	ss << '\r';	break;
					case 't':	ss << '\t';	break;
					case '"':	ss << '\"'; break;
					case '\\':	ss << '\\'; break;
				}

				continue;
			}

			ss << tmp[i];
		}

		return new LitString(st.ploc(), ss.str(), israw);
	}

	LitArray* parseArray(State& st, bool israw)
	{
		iceAssert(st.front() == TT::LSquare);
		Token front = st.eat();

		std::vector<Expr*> values;
		while(true)
		{
			Token tok = st.frontAfterWS();
			if(tok.type == TT::Comma)
			{
				st.pop();

				if(st.frontAfterWS() == TT::RSquare)
					error(tok.loc, "Trailing commas are not allowed");

				continue;
			}
			else if(tok.type == TT::RSquare)
			{
				break;
			}
			else
			{
				st.skipWS();
				values.push_back(parseExpr(st));
			}
		}

		st.skipWS();
		iceAssert(st.front() == TT::RSquare);

		auto end = st.eat();

		auto ret = new LitArray(front.loc, values);
		ret->raw = israw;

		ret->loc.col = front.loc.col + 1;
		ret->loc.len = (end.loc.col - front.loc.col) + 1;
		return ret;
	}
}

















































