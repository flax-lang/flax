// literal.cpp
// Copyright (c) 2014 - 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include "defs.h"
#include "platform.h"
#include "parser_internal.h"

#include "memorypool.h"

#include "utf8rewind/include/utf8rewind/utf8rewind.h"

#include <sstream>

using namespace ast;
using namespace lexer;


using TT = lexer::TokenType;
namespace parser
{
	LitNumber* parseNumber(State& st)
	{
		iceAssert(st.front() == TT::IntegerNumber || st.front() == TT::FloatingNumber);
		auto t = st.eat();

		return util::pool<LitNumber>(st.ploc(), t.str(), /* floating: */ t == TT::FloatingNumber);
	}

	static std::string parseHexEscapes(const Location& loc, std::string_view sv, size_t* ofs)
	{
		if(sv[0] == 'x')
		{
			if(sv.size() < 3)
				error(loc, "malformed escape sequence: unexpected end of string");

			if(!isxdigit(sv[1]) || !isxdigit(sv[2]))
				error(loc, "malformed escape sequence: non-hex character in \\x escape");

			// ok then.
			char s[2] = { sv[1], sv[2] };
			char val = static_cast<char>(std::stol(s, /* pos: */ 0, /* base: */ 16));

			*ofs = 3;
			return std::string(&val, 1);
		}
		else if(sv[0] == 'u')
		{
			if(sv.size() < 3)
				error(loc, "malformed escape sequence: unexpected end of string");

			sv.remove_prefix(1);
			if(sv[0] != '{')
				error(loc, "malformed escape sequence: expected '{' after \\u");

			sv.remove_prefix(1);

			std::string digits;
			size_t i = 0;
			for(i = 0; i < sv.size(); i++)
			{
				if(sv[i] == '}') break;

				if(!isxdigit(sv[i]))
					error(loc, "malformed escape sequence: non-hex character '%c' inside \\u{...}", sv[i]);

				if(digits.size() == 8)
					error(loc, "malformed escape sequence: too many digits inside \\u{...}; up to 8 are allowed");

				digits += sv[i];
			}

			if(sv[i] != '}')
				error(loc, "malformed escape sequence: expcected '}' to end codepoint escape");

			uint32_t codepoint = std::stol(digits, /* pos: */ 0, /* base: */ 16);

			char output[8] = { 0 };
			int err = 0;
			auto sz = utf32toutf8(&codepoint, 4, output, 8, &err);
			if(err != UTF8_ERR_NONE)
				error(loc, "invalid utf32 codepoint!");

			*ofs = 3 + digits.size();
			return std::string(output, sz);
		}
		else
		{
			iceAssert("wtf yo" && 0);
			return "";
		}
	}

	std::string parseStringEscapes(const Location& loc, const std::string& str)
	{
		std::stringstream ss;

		for(size_t i = 0; i < str.length(); i++)
		{
			if(str[i] == '\\')
			{
				i++;
				switch(str[i])
				{
					case 'n':   ss << "\n"; break;
					case 'b':   ss << "\b"; break;
					case 'r':   ss << "\r"; break;
					case 't':   ss << "\t"; break;
					case '"':   ss << "\""; break;
					case '\\':  ss << "\\"; break;

					case 'x':   // fallthrough
					case 'u': {
						size_t ofs = 0;
						ss << parseHexEscapes(loc, std::string_view(str.c_str() + i, str.size() - i), &ofs);
						i += ofs - 1;
						break;
					}

					default:
						ss << std::string("\\") + str[i];
						break;
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

		return util::pool<LitString>(st.ploc(), parseStringEscapes(st.ploc(), t.str()), israw);
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

















































