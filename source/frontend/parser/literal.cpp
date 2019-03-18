// literal.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "defs.h"
#include "platform.h"
#include "parser_internal.h"

#include "mpool.h"

#include <sstream>

using namespace ast;
using namespace lexer;

// #ifdef _WIN32
// 	#define PLATFORM_NEWLINE "\r\n"
// #else
// 	#define PLATFORM_NEWLINE "\n"
// #endif

using TT = lexer::TokenType;
namespace parser
{
	LitNumber* parseNumber(State& st)
	{
		iceAssert(st.front() == TT::Number);
		auto t = st.eat();

		return util::pool<LitNumber>(st.ploc(), t.str());
	}

	std::string parseStringEscapes(const std::string& str)
	{
		std::stringstream ss;

		for(size_t i = 0; i < str.length(); i++)
		{
			if(str[i] == '\\')
			{
				i++;
				switch(str[i])
				{
					// todo: handle hex sequences and stuff
					case 'n':   ss << "\n"; break;
					case 'b':   ss << "\b"; break;
					case 'r':   ss << "\r"; break;
					case 't':   ss << "\t"; break;
					case '"':   ss << "\""; break;
					case '\\':  ss << "\\"; break;
					default:    ss << std::string("\\") + str[i]; break;
				}
			}
			else
			{
				ss << str[i];
			}
		}

		return ss.str();
	}

	LitString* parseString(State& st, bool israw)
	{
		iceAssert(st.front() == TT::StringLiteral);
		auto t = st.eat();

		return util::pool<LitString>(st.ploc(), parseStringEscapes(t.str()), israw);
	}

	LitArray* parseArray(State& st, bool israw)
	{
		iceAssert(st.front() == TT::LSquare);
		Token front = st.eat();

		pts::Type* explType = 0;
		if(st.front() == TT::As)
		{
			// get an explicit type.
			st.pop();
			explType = parseType(st);

			if(st.pop() != TT::Colon)
				expectedAfter(st.ploc(), ":", "explicit type in array literal", st.prev().str());
		}

		std::vector<Expr*> values;
		while(true)
		{
			Token tok = st.frontAfterWS();
			if(tok.type == TT::Comma)
			{
				st.pop();

				if(st.frontAfterWS() == TT::RSquare)
					error(tok.loc, "trailing commas are not allowed");

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

		auto ret = util::pool<LitArray>(front.loc, values);
		ret->explicitType = explType;               // don't matter if null.
		ret->raw = israw;

		// ret->loc.col = front.loc.col + 1;
		ret->loc.len = (end.loc.col - front.loc.col) + 1;
		return ret;
	}
}

















































