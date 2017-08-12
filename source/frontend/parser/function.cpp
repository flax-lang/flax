// function.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "defs.h"
#include "parser_internal.h"

using namespace ast;
using namespace lexer;

using TT = TokenType;
namespace parser
{
	FuncDefn* parseFunction(State& st)
	{
		iceAssert(st.eat() == TT::Func);
		if(st.front() != TT::Identifier)
			expectedAfter(st, "identifier", "'fn'", st.front().str());

		FuncDefn* defn = new FuncDefn(st.loc());
		defn->name = st.eat().str();

		// check for generic function
		if(st.front() == TT::LAngle)
		{
			st.eat();
			// parse generic
			if(st.front() == TT::RAngle)
				error(st, "Empty type parameter lists are not allowed");

			defn->generics = parseGenericTypeList(st);
		}

		if(st.front() != TT::LParen)
			expectedAfter(st, "'('", "function declaration to begin argument list", st.front().str());


		return 0;
	}











	std::map<std::string, TypeConstraints_t> parseGenericTypeList(State& st)
	{
		std::map<std::string, TypeConstraints_t> ret;

		while(st.front().type != TT::RAngle)
		{
			if(st.front().type == TT::Identifier)
			{
				std::string gt = st.eat().text.to_string();
				TypeConstraints_t constrs;

				if(st.front().type == TT::Colon)
				{
					st.eat();
					if(st.front().type != TT::Identifier)
						error(st, "Expected identifier after beginning of type constraint list");

					while(st.front().type == TT::Identifier)
					{
						constrs.protocols.push_back(st.eat().text.to_string());

						if(st.front().type == TT::Ampersand)
						{
							st.eat();
						}
						else if(st.front().type != TT::Comma && st.front().type != TT::RAngle)
						{
							error(st, "Expected ',' or '>' to end type parameter list (1)");
						}
					}
				}
				else if(st.front().type != TT::Comma && st.front().type != TT::RAngle)
				{
					error(st, "Expected ',' or '>' to end type parameter list (2)");
				}

				ret[gt] = constrs;
			}
			else if(st.front().type == TT::Comma)
			{
				st.eat();
			}
			else if(st.front().type != TT::RAngle)
			{
				error(st, "Expected '>' to end type parameter list");
			}
		}

		iceAssert(st.eat().type == TT::RAngle);

		return ret;
	}
}
