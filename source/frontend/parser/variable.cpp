// variable.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "pts.h"
#include "parser_internal.h"

using namespace ast;
using namespace lexer;

namespace parser
{
	std::map<size_t, std::tuple<std::string, bool, Location>> parseArrayDecomp(State& st)
	{
		using TT = lexer::TokenType;
		iceAssert(st.front() == TT::LSquare);
		st.pop();

		std::map<size_t, std::tuple<std::string, bool, Location>> mapping;

		size_t index = 0;
		while(st.front() != TT::RSquare)
		{
			std::string id;
			bool ref = false;

			if(st.front() == TT::Ampersand)
				ref = true, st.pop();

			if(st.front() == TT::Underscore || (st.front() == TT::Identifier && st.front().str() == "_"))
			{
				id = "_";
				st.pop();

				if(ref) error(st, "Invalid combination of '&' and '_' in array decomposition");
			}
			else if(st.front() == TT::Identifier)
			{
				mapping[index] = std::make_tuple(id, ref, st.loc());
				index++;

				st.eat();
			}
			else if(st.front() == TT::Ellipsis)
			{
				st.eat();

				// check if we're binding
				if(st.front() == TT::Ampersand || st.front() == TT::Identifier)
				{
					// yes we are
					if(st.front() == TT::Ampersand)
					{
						ref = true;
						st.pop();
						if(st.front() != TT::Identifier || st.front().str() == "_")
							expectedAfter(st, "identifier", "'&' in binding", st.front().str());
					}

					iceAssert(st.front() == TT::Identifier);
					id = st.front().str();

					mapping[-1] = std::make_tuple(id, ref, st.loc());
					st.pop();
				}
				else if(st.front() != TT::RSquare)
				{
					error("Ellipsis binding must be last in decomposition, found '%s' instead of expected ']'", st.front().str().c_str());
				}
			}
			else
			{
				error(st, "Unexpected token '%s' in array decomposition", st.front().str());
			}

			if(st.front() == TT::Comma)
			{
				st.eat();
				if(st.front() == TT::RSquare)
					error(st, "Trailing commas are not allowed");

				continue;
			}
		}

		iceAssert(st.eat() == TT::RSquare);
		return mapping;
	}

	ArrayDecompVarDefn* parseArrayDecompDecl(State& st)
	{
		using TT = lexer::TokenType;
		iceAssert(st.front() == TT::LSquare);

		auto decomp = new ArrayDecompVarDefn(st.loc());
		decomp->mapping = parseArrayDecomp(st);

		if(st.front() != TT::Equal)
			expected(st, "'=' for assignment to decomposition", st.front().str());

		st.pop();
		decomp->initialiser = parseExpr(st);

		return decomp;
	}












	std::vector<ast::TupleDecompMapping> parseTupleDecomp(State& st)
	{
		error("unsupported");
	}

	static ast::TupleDecompVarDefn* parseTupleDecompDecl(State& st)
	{
		using TT = lexer::TokenType;
		iceAssert(st.front() == TT::LParen);

		auto decomp = new TupleDecompVarDefn(st.loc());
		decomp->mappings = parseTupleDecomp(st);

		if(st.front() != TT::Equal)
			expected(st, "'=' for assignment to decomposition", st.front().str());

		st.pop();
		decomp->initialiser = parseExpr(st);

		return decomp;
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
