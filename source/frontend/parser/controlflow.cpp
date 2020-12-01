// controlflow.cpp
// Copyright (c) 2014 - 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "parser_internal.h"

#include "memorypool.h"

namespace parser
{
	using TT = lexer::TokenType;
	using namespace ast;

	PResult<Stmt> parseIfStmt(State& st)
	{
		using Case = IfStmt::Case;

		auto tok_if = st.eat();
		iceAssert(tok_if == TT::If || tok_if == TT::Directive_If);

		bool isStaticIf = tok_if == TT::Directive_If;

		// of this form:
		// if(var x = 30; var k = 30; x == k) { ... } else if(cond) { ... }
		// braces are compulsory
		// parentheses around the condition are not

		Block* elseCase = 0;
		std::vector<Case> cases;
		cases.push_back(Case());


		// first one
		{
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

			if(auto x = parseBracedBlock(st); x.isError())
				return PResult<Stmt>::copyError(x);

			else
				cases.back().body = x.val();
		}


		// ok, do the else-if chain
		while(st.frontAfterWS() == TT::Else)
		{
			st.eat();

			if(st.front() == TT::If)
			{
				Case c;

				// ok, it's an else-if.
				st.eat();

				while(st.front() == TT::Val || st.front() == TT::Var)
				{
					c.inits.push_back(parseVariable(st));
					st.skipWS();

					if(st.front() != TT::Semicolon)
						expectedAfter(st, "semicolon ';'", "if-variable-initialisation", st.front().str());

					st.eat();
				}

				c.cond = parseExpr(st);

				if(auto x = parseBracedBlock(st); x.isError())
					return PResult<Stmt>::copyError(x);

				else
					c.body = x.val();

				cases.push_back(c);
			}
			else if(st.frontAfterWS() == TT::LBrace || st.frontAfterWS() == TT::FatRightArrow)
			{
				// ok, parse an else
				elseCase = parseBracedBlock(st).val();
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
			auto ret = util::pool<IfDirective>(tok_if.loc);
			ret->cases = cases;
			ret->elseCase = elseCase;

			for(auto& c : ret->cases)
				c.body->doNotPushNewScope = true;

			if(ret->elseCase)
				ret->elseCase->doNotPushNewScope = true;

			return ret;
		}
		else
		{
			// normal runtime if
			auto ret = util::pool<IfStmt>(tok_if.loc);
			ret->cases = cases;
			ret->elseCase = elseCase;

			return ret;
		}
	}

	ReturnStmt* parseReturn(State& st)
	{
		iceAssert(st.front() == TT::Return);

		auto ret = util::pool<ReturnStmt>(st.loc());
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






	Stmt* parseForLoop(State& st)
	{
		iceAssert(st.front() == TT::For);
		st.eat();

		if(!zfu::match(st.front(), TT::Identifier, TT::LParen))
			expectedAfter(st.loc(), "'(' or identifier", "'for'", st.front().str());

		auto ret = util::pool<ForeachLoop>(st.ploc());

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
		ret->body = parseBracedBlock(st).val();
		return ret;
	}



	PResult<WhileLoop> parseWhileLoop(State& st)
	{
		// 1. do { }			-- body = block, cond = 0, doVariant = true
		// 2. while x { }		-- body = block, cond = x, doVariant = false
		// 3. do { } while x	-- body = block, cond = x, doVariant = true

		auto loc = st.loc();

		Expr* cond = 0;
		Block* body = 0;
		bool isdo = false;

		iceAssert(st.front() == TT::While || st.front() == TT::Do);

		if(st.front() == TT::While)
		{
			st.eat();
			cond = parseExpr(st);
			st.skipWS();

			auto res = parseBracedBlock(st);
			if(res.isError())
				return PResult<WhileLoop>::copyError(res);

			body = res.val();
		}
		else
		{
			isdo = true;

			st.eat();

			auto res = parseBracedBlock(st);
			if(res.isError())
				return PResult<WhileLoop>::copyError(res);

			body = res.val();

			if(st.front() == TT::While)
			{
				st.eat();

				// do a check for stupid "do { } while { }"
				if(st.frontAfterWS() == TT::LBrace)
					expected(st.frontAfterWS().loc, "conditional expression after while", st.frontAfterWS().str());

				cond = parseExpr(st);
			}
		}

		auto ret = util::pool<WhileLoop>(loc);
		ret->isDoVariant = isdo;
		ret->body = body;
		ret->cond = cond;

		return ret;
	}


	Stmt* parseBreak(State& st)
	{
		iceAssert(st.front() == TT::Break);
		return util::pool<BreakStmt>(st.eat().loc);
	}

	Stmt* parseContinue(State& st)
	{
		iceAssert(st.front() == TT::Continue);
		return util::pool<ContinueStmt>(st.eat().loc);
	}
}




















