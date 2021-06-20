// misc.cpp
// Copyright (c) 2014 - 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include "frontend.h"
#include "parser_internal.h"

#include "memorypool.h"

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



	RunDirective* parseRunDirective(State& st)
	{
		iceAssert(st.front() == TT::Directive_Run);

		auto ret = util::pool<RunDirective>(st.eat().loc);

		// trick the parser. when we do a #run, we will run it in a function wrapper, so we
		// are technically "inside" a function.
		st.enterFunctionBody();
		defer(st.leaveFunctionBody());

		if(st.front() == TT::LBrace)    ret->block = parseBracedBlock(st).val();
		else                            ret->insideExpr = parseExpr(st);

		return ret;
	}



	AttribSet parseAttributes(State& st)
	{
		using UA = AttribSet::UserAttrib;

		if(st.front() != TT::At && (st.front() <= TT::Attr_ATTRS_BEGIN || st.front() >= TT::Attr_ATTRS_END))
			return AttribSet::of(attr::NONE);

		auto parseUA = [](State& st) -> UA {

			iceAssert(st.front() == TT::At);
			st.pop();

			auto ret = UA(st.eat().str(), {});

			if(st.front() == TT::LSquare)
			{
				auto begin = st.loc();

				st.eat();

				//* this means that attributes can only take tokens as arguments. if you want more complex stuff,
				//* then it needs to be wrapped up in a string literal.
				while(st.front() != TT::RSquare)
				{
					ret.args.push_back(st.eat().str());

					if(st.front() == TT::Comma)
						st.eat();

					else if(st.front() != TT::RSquare)
						expected(st.loc(), "']' to end argument list", st.prev().str());
				}

				iceAssert(st.front() == TT::RSquare);
				st.pop();

				if(ret.args.empty())
					warn(Location::unionOf(begin, st.ploc()), "empty argument list to attribute");
			}

			return ret;
		};


		AttribSet ret;
		while(true)
		{
			// i would love me some static reflection right now
			switch(st.front())
			{
				case TT::Attr_Raw:      ret.set(attr::RAW); st.pop(); break;
				case TT::Attr_Packed:   ret.set(attr::PACKED); st.pop(); break;
				case TT::Attr_NoMangle: ret.set(attr::NO_MANGLE); st.pop(); break;
				case TT::Attr_EntryFn:  ret.set(attr::FN_ENTRYPOINT); st.pop(); break;
				case TT::Attr_Operator: unexpected(st.loc(), "@operator declaration");

				case TT::At:
					ret.add(parseUA(st));
					break;

				default:
					goto out;
			}
		}

		// sue me
	out:
		return ret;
	}
}













