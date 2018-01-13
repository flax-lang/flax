// variable.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "pts.h"
#include "parser_internal.h"

using namespace ast;
using namespace lexer;

namespace parser
{
	static ast::DecompMapping recursivelyParseDecomp(State& st)
	{
		using TT = lexer::TokenType;
		ast::DecompMapping outer;

		iceAssert(st.front() == TT::LSquare || st.front() == TT::LParen);
		if(st.pop() == TT::LSquare)
			outer.array = true;

		bool didRest = false;
		while(st.front() != (outer.array ? TT::RSquare : TT::RParen))
		{
			ast::DecompMapping inside;

			if(st.front() == TT::Ampersand)
				inside.ref = true, st.pop();

			if(st.front() == TT::LParen || st.front() == TT::LSquare)
			{
				if(inside.ref) error(st, "Cannot bind by reference in nested decomposition; modify the binding type for each identifier");

				inside = recursivelyParseDecomp(st);
			}
			else if(st.front() == TT::Ellipsis)
			{
				if(!outer.array)    error(st, "'...' cannot be used in a tuple destructure");
				else if(didRest)    error(st, "'...' must be the last binding in an array destructure");
				else if(inside.ref) error(st, "Invalid use of '&' before '...' binding");

				st.pop();
				if(st.front() == TT::Ampersand)
					outer.restRef = true, st.pop();

				if(st.front() == TT::Identifier)
					outer.restName = st.front().str(), st.pop();

				// else
				// 	expected(st, "binding for rest of array after '...' in destructuring declaration", st.front().str());

				if(outer.restName == "_" && outer.restRef)
					error(st.ploc(), "Invalid combination of '_' and '&'");

				didRest = true;
			}
			else if(st.front() == TT::Identifier)
			{
				inside.name = st.pop().str();
				if(inside.name == "_" && inside.ref)
					error(st.loc(), "Invalid combination of '_' and '&'");

				outer.inner.push_back(inside);
			}
			else
			{
				expected(st.loc(), "identifier or '...' in destructuring declaration", st.front().str());
			}

			if(st.front() != TT::Comma && st.front() != (outer.array ? TT::RSquare : TT::RParen))
				expected(st, "')', ']' or ',' in destructuring declaration", st.front().str());

			if(st.front() == TT::Comma)
				st.pop();
		}

		if(outer.array && !didRest)
			error(st, "'...' is mandatory for array destructuring, regardless of binding");

		iceAssert(st.front() == (outer.array ? TT::RSquare : TT::RParen));
		st.pop();

		return outer;
	}



	ArrayDecompVarDefn* parseArrayDecompDecl(State& st)
	{
		using TT = lexer::TokenType;
		iceAssert(st.front() == TT::LSquare);

		auto decomp = new ArrayDecompVarDefn(st.loc());
		decomp->bindings = recursivelyParseDecomp(st);

		if(st.front() != TT::Equal)
			expected(st, "'=' for assignment to decomposition", st.front().str());

		st.pop();
		decomp->initialiser = parseExpr(st);

		return decomp;
	}


	static ast::TupleDecompVarDefn* parseTupleDecompDecl(State& st)
	{
		using TT = lexer::TokenType;
		iceAssert(st.front() == TT::LParen);

		auto decomp = new TupleDecompVarDefn(st.loc());
		decomp->bindings = parseTupleDecomp(st);

		if(st.front() != TT::Equal)
			expected(st, "'=' for assignment to decomposition", st.front().str());

		st.pop();
		decomp->initialiser = parseExpr(st);

		return decomp;
	}

	ast::DecompMapping parseTupleDecomp(State& st)
	{
		return recursivelyParseDecomp(st);
	}














	Stmt* parseVariable(State& st)
	{
		using TT = lexer::TokenType;
		auto loc = st.front().loc;

		iceAssert(st.front() == TT::Var || st.front() == TT::Val);

		bool isImmut = (st.eat() == TT::Val);
		if(st.front() == TT::LParen)
		{
			auto ret = parseTupleDecompDecl(st);
			ret->immut = isImmut;

			return ret;
		}
		else if(st.front() == TT::LSquare)
		{
			auto ret = parseArrayDecompDecl(st);
			ret->immut = isImmut;

			return ret;
		}
		else if(st.front() != TT::Identifier)
		{
			expectedAfter(st, "identifier", "'" + std::string(isImmut ? "val" : "var") + "'", st.front().str());
		}

		std::string name = st.eat().str();
		pts::Type* type = pts::InferredType::get();
		Expr* value = 0;

		if(st.front() == TT::Colon)
		{
			st.pop();
			type = parseType(st);
		}
		else if(st.front() != TT::Equal && type == pts::InferredType::get())
		{
			error(st, "Expected initial value for type inference on variable '%s'", name);
		}

		if(st.front() == TT::Equal)
		{
			st.pop();
			value = parseExpr(st);
		}
		else if(isImmut)
		{
			error(st, "Expected initial value for immutable variable '%s'", name);
		}

		auto ret = new VarDefn(loc);
		ret->initialiser = value;
		ret->immut = isImmut;
		ret->type = type;
		ret->name = name;

		return ret;
	}



}
