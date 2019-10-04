// controlflow.cpp
// Copyright (c) 2014 - 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "parser_internal.h"

#include "memorypool.h"

namespace parser
{
	using TT = lexer::TokenType;
	ast::Stmt* parseIfStmt(State& st)
	{
		using Case = ast::IfStmt::Case;

		auto tok_if = st.eat();
		iceAssert(tok_if == TT::If || tok_if == TT::Directive_If);

		bool isStaticIf = tok_if == TT::Directive_If;

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
			else if(st.frontAfterWS() == TT::LBrace || st.frontAfterWS() == TT::FatRightArrow)
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

		if(isStaticIf)
		{
			// compile-time if
			auto ret = util::pool<ast::IfDirective>(tok_if.loc);
			ret->cases = cases;
			ret->elseCase = elseCase;

			for(auto& c : ret->cases)
				c.body->doNotPushNewScope = true;

			ret->elseCase->doNotPushNewScope = true;

			return ret;
		}
		else
		{
			// normal runtime if
			auto ret = util::pool<ast::IfStmt>(tok_if.loc);
			ret->cases = cases;
			ret->elseCase = elseCase;

			return ret;
		}
	}

	ast::ReturnStmt* parseReturn(State& st)
	{
		iceAssert(st.front() == TT::Return);

		auto ret = util::pool<ast::ReturnStmt>(st.loc());
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






	ast::Stmt* parseForLoop(State& st)
	{
		iceAssert(st.front() == TT::For);
		st.eat();

		if(!util::match(st.front(), TT::Identifier, TT::LParen))
			expectedAfter(st.loc(), "'(' or identifier", "'for'", st.front().str());

		auto ret = util::pool<ast::ForeachLoop>(st.ploc());

		if(st.front() == TT::LParen)
		{
			ret->bindings = parseTupleDecomp(st);
		}
		else if(st.front() == TT::Ampersand || st.front() == TT::Identifier)
		{
			if(st.front() == TT::Ampersand)
				ret->bindings.ref = true, st.pop();

			ret->bindings.loc = st.loc();
			ret->bindings.name = st.eat().str();
		}
		else
		{
			expectedAfter(st.loc(), "one or more variable bindings", "'for'", st.front().str());
		}

		// check for the comma for the index variable
		if(st.front() == TT::Comma)
		{
			st.pop();
			if(st.front() != TT::Identifier)
				expectedAfter(st.loc(), "identifier for index binding", "',' in for loop", st.front().str());

			else if(st.front().str() == "_")
				error("redundant use of '_' in binding for index variable in for loop; omit the binding entirely to ignore the index variable");

			ret->indexVar = st.eat().str();
		}

		if(st.front() != TT::Identifier || st.front().str() != "in")
			expected(st.loc(), "'in' in for-loop", st.front().str());

		st.eat();

		// get the array.
		ret->array = parseExpr(st);

		st.skipWS();
		ret->body = parseBracedBlock(st);
		return ret;
	}



	ast::WhileLoop* parseWhileLoop(State& st)
	{
		// 1. do { }			-- body = block, cond = 0, doVariant = true
		// 2. while x { }		-- body = block, cond = x, doVariant = false
		// 3. do { } while x	-- body = block, cond = x, doVariant = true

		auto loc = st.loc();

		ast::Expr* cond = 0;
		ast::Block* body = 0;
		bool isdo = false;

		iceAssert(st.front() == TT::While || st.front() == TT::Do);

		if(st.front() == TT::While)
		{
			st.eat();
			cond = parseExpr(st);
			st.skipWS();

			body = parseBracedBlock(st);
		}
		else
		{
			isdo = true;

			st.eat();

			body = parseBracedBlock(st);

			if(st.front() == TT::While)
			{
				st.eat();

				// do a check for stupid "do { } while { }"
				if(st.frontAfterWS() == TT::LBrace)
					expected(st.frontAfterWS().loc, "conditional expression after while", st.frontAfterWS().str());

				cond = parseExpr(st);
			}
		}

		auto ret = util::pool<ast::WhileLoop>(loc);
		ret->isDoVariant = isdo;
		ret->body = body;
		ret->cond = cond;

		return ret;
	}


	ast::Stmt* parseBreak(State& st)
	{
		iceAssert(st.front() == TT::Break);
		return util::pool<ast::BreakStmt>(st.eat().loc);
	}

	ast::Stmt* parseContinue(State& st)
	{
		iceAssert(st.front() == TT::Continue);
		return util::pool<ast::ContinueStmt>(st.eat().loc);
	}
}




















