// controlflow.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "parser_internal.h"

namespace parser
{
	using TT = lexer::TokenType;
	ast::IfStmt* parseIfStmt(State& st)
	{
		using Case = ast::IfStmt::Case;

		auto tok_if = st.eat();
		iceAssert(tok_if == TT::If);

		// of this form:
		// if(var x = 30; var k = 30; x == k) { ... } else if(cond) { ... }
		// braces are compulsory
		// parentheses around the condition are not

		ast::Block* elseCase = 0;
		std::vector<Case> cases;
		cases.push_back(Case());


		// first one
		bool hadParen = false;
		{
			if(st.front() == TT::LParen)
				hadParen = true, st.eat();

			// ok -- the first one we must do manually.
			// probably not, but i'm lazy.
			while(st.front() == TT::Val || st.front() == TT::Var)
			{
				cases.back().inits.push_back(parseVariable(st));
				st.skipWS();

				if(st.front() != TT::Semicolon)
					expectedAfter(st, "semicolon ';'", "if-variable-initialisation", st.front().str());

				st.eat();
			}

			// ok, we finished doing the variables.
			cases.back().cond = parseExpr(st);
			if(hadParen)
			{
				if(st.front() != TT::RParen)
					expected(st, "closing parenthesis ')'", st.front().str());

				st.eat();
			}
			hadParen = false;

			cases.back().body = parseBracedBlock(st);
		}


		// ok, do the else-if chain
		while(st.frontAfterWS() == TT::Else)
		{
			st.eat();
			bool paren = false;

			if(st.front() == TT::If)
			{
				Case c;

				// ok, it's an else-if.
				st.eat();
				if(st.front() == TT::LParen)
					paren = true, st.eat();

				while(st.front() == TT::Val || st.front() == TT::Var)
				{
					c.inits.push_back(parseVariable(st));
					st.skipWS();

					if(st.front() != TT::Semicolon)
						expectedAfter(st, "semicolon ';'", "if-variable-initialisation", st.front().str());

					st.eat();
				}

				c.cond = parseExpr(st);
				if(paren)
				{
					if(st.front() != TT::RParen)
						expected(st, "closing parenthesis ')'", st.front().str());

					st.eat();
				}

				c.body = parseBracedBlock(st);
				cases.push_back(c);
			}
			else if(st.frontAfterWS() == TT::LBrace)
			{
				// ok, parse an else
				elseCase = parseBracedBlock(st);
				break;
			}
			else
			{
				// um.
				expectedAfter(st, "'if' or '{'", "'else'", st.frontAfterWS().str());
			}
		}

		auto ret = new ast::IfStmt(tok_if.loc);
		ret->cases = cases;
		ret->elseCase = elseCase;

		return ret;
	}

	ast::ReturnStmt* parseReturn(State& st)
	{
		iceAssert(st.front() == TT::Return);

		auto ret = new ast::ReturnStmt(st.loc());
		st.eat();

		// check what's the next thing. problem: return has an *optional* value
		// solution: the value must not be separated by any whitespace or comment tokens
		if(st.front() != TT::Comment && st.front() != TT::NewLine && st.front() != TT::RBrace && st.front() != TT::EndOfFile)
		{
			ret->value = parseExpr(st);
		}
		else
		{
			// do nothing
		}

		return ret;
	}
}














