// misc.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "parser_internal.h"

using namespace ast;
using namespace lexer;

namespace parser
{
	ImportStmt* parseImport(State& st)
	{
		using TT = TokenType;
		iceAssert(st.eat() == TT::Import);

		if(st.frontAfterWS() == TT::StringLiteral)
		{
			return new ImportStmt(st.loc(), st.frontAfterWS().str());
		}
		else if(st.frontAfterWS() == TT::Identifier)
		{
			std::string name;
			auto loc = st.loc();
			while(st.front() == TokenType::Identifier)
			{
				name += st.eat().str();

				if(st.front() == TokenType::Period)
				{
					name += "/";
					st.eat();
				}
				else if(st.frontIsWS() || st.front() == TT::Semicolon)
				{
					break;
				}
				else
				{
					error(st, "Unexpected token '%s' in module specifier for import statement",
						st.front().str().c_str());
				}
			}

			// i hope this works.
			return new ImportStmt(loc, name);
		}
		else
		{
			error(st, "Expected either string literal or identifer after 'import' for module specifier, found '%s' instead",
				st.frontAfterWS().str().c_str());
		}
	}
}
