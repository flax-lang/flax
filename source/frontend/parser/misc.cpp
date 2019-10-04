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

		if(st.front() == TT::LBrace)    ret->block = parseBracedBlock(st);
		else                            ret->insideExpr = parseExpr(st);

		return ret;
	}




	PlatformDefn* parsePlatformDefn(State& st)
	{
		iceAssert(st.front() == TT::Attr_Platform);
		auto l = st.loc();

		st.eat();

		if(st.eat() != TT::LSquare)
			expectedAfter(st.ploc(), "'['", "@platform definition", st.prev().str());

		PlatformDefn* pd = util::pool<PlatformDefn>(l);

		// see what the thing is.
		if(st.front() == TT::Identifier && st.front().str() == "intrinsic")
		{
			st.eat();
			pd->defnType = PlatformDefn::Type::Intrinsic;

			if(st.eat() != TT::Comma)
				expected(st.ploc(), "',' in argument list to @platform", st.prev().str());

			if(st.front() != TT::StringLiteral)
				expected(st.loc(), "string literal to specify intrinsic name", st.front().str());

			auto realname = st.eat().str();

			if(st.eat() != TT::RSquare)
				expectedAfter(st.ploc(), "']'", "@platform definition", st.prev().str());

			if(st.front() != TT::Func)
				expectedAfter(st.loc(), "function declaration", "@platform", st.front().str());

			auto [ defn, isvar, varloc ] = parseFunctionDecl(st);
			(void) varloc;

			if(!defn->generics.empty())
				error(defn->loc, "platform intrinsics cannot be generic");

			auto ffn = util::pool<ForeignFuncDefn>(st.loc());
			ffn->realName = realname;

			ffn->loc = defn->loc;
			ffn->isVarArg = isvar;
			ffn->name = defn->name;
			ffn->params = defn->params;
			ffn->visibility = defn->visibility;
			ffn->returnType = defn->returnType;


			pd->intrinsicDefn = ffn;
		}
		else if(st.front() == TT::Identifier && st.front().str() == "integer_type")
		{
			st.eat();
			pd->defnType = PlatformDefn::Type::IntegerType;

			if(st.eat() != TT::Comma)
				expected(st.ploc(), "',' in argument list to @platform", st.prev().str());

			auto num = st.front().str();
			if(st.front() != TT::Number || num.find('.') != std::string::npos)
				expected(st.ploc(), "integer value to specify type size (in bits)", st.front().str());

			st.eat();

			int sz = std::stoi(num);
			if(sz <= 0)     expected(st.ploc(), "non-zero and non-negative size", num);
			else if(sz < 8) error(st.ploc(), "types less than 8-bits wide are currently not supported");

			pd->typeSizeInBits = sz;

			if(st.eat() != TT::RSquare)
				expectedAfter(st.ploc(), "']'", "@platform definition", st.prev().str());

			if(st.front() != TT::Identifier)
				expectedAfter(st.loc(), "identifier as type name", "@platform definition", st.front().str());

			pd->typeName = st.eat().str();
		}
		else if(st.front() == TT::Identifier && st.front().str() == "native_word_size")
		{
			if(!st.nativeWordSizeStillValid)
			{
				SimpleError::make(st.loc(), "setting the native word size is no longer possible at this point")->append(
					BareError::make(MsgType::Note, "@platform[native_word_size] must appear before any code declarations, "
						"and be the first '@platform' declaration"))->postAndQuit();
			}

			st.eat();

			if(st.eat() != TT::RSquare)
				expectedAfter(st.ploc(), "']'", "@platform definition", st.prev().str());

			auto num = st.front().str();
			if(st.front() != TT::Number || num.find('.') != std::string::npos)
				expected(st.ploc(), "integer value to specify word size (in bits)", st.front().str());

			st.eat();

			int sz = std::stoi(num);
			if(sz <= 0)     expected(st.ploc(), "non-zero and non-negative size", num);
			else if(sz < 8) error(st.ploc(), "types less than 8-bits wide are currently not supported");

			//? should we warn if it was already set?
			st.cState->nativeWordSize = sz;

			return 0;
		}
		else
		{
			error(st.loc(), "invalid platform declaration of type '%s'", st.front().str());
		}

		return pd;
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













