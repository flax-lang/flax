// variable.cpp
// Copyright (c) 2014 - 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include "pts.h"
#include "parser_internal.h"

#include "memorypool.h"

#include <set>

using namespace ast;
using namespace lexer;

namespace parser
{
	static DecompMapping recursivelyParseDecomp(State& st)
	{
		using TT = lexer::TokenType;
		DecompMapping outer;

		outer.loc = st.loc();

		iceAssert(st.front() == TT::LSquare || st.front() == TT::LParen);
		if(st.pop() == TT::LSquare)
			outer.array = true;

		bool didRest = false;
		while(st.front() != (outer.array ? TT::RSquare : TT::RParen))
		{
			DecompMapping inside;
			inside.loc = st.loc();

			if(st.front() == TT::Ampersand)
				inside.ref = true, st.pop();

			if(st.front() == TT::LParen || st.front() == TT::LSquare)
			{
				if(inside.ref) error(st, "cannot bind by reference in nested decomposition; modify the binding type for each identifier");

				outer.inner.push_back(recursivelyParseDecomp(st));
			}
			else if(st.front() == TT::Ellipsis)
			{
				if(!outer.array)    error(st, "'...' cannot be used in a tuple destructure");
				else if(didRest)    error(st, "'...' must be the last binding in an array destructure");
				else if(inside.ref) error(st, "invalid use of '&' before '...' binding");

				st.pop();
				if(st.front() == TT::Ampersand)
					outer.restRef = true, st.pop();

				if(st.front() == TT::Identifier)
					outer.restName = st.front().str(), st.pop();

				if(outer.restName == "_" && outer.restRef)
					error(st.ploc(), "invalid combination of '_' and '&'");

				didRest = true;
			}
			else if(st.front() == TT::Identifier)
			{
				inside.name = st.pop().str();
				if(inside.name == "_" && inside.ref)
					error(st.loc(), "invalid combination of '_' and '&'");

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



	DecompVarDefn* parseDecompDecl(State& st)
	{
		using TT = lexer::TokenType;
		iceAssert(st.front() == TT::LSquare || st.front() == TT::LParen);

		auto decomp = util::pool<DecompVarDefn>(st.loc());
		decomp->bindings = recursivelyParseDecomp(st);

		// we need to ensure there're no duplicate names.
		std::function<void (const DecompMapping& dm, std::map<std::string, Location>& names)> visit;

		visit = [&visit](const DecompMapping& dm, std::map<std::string, Location>& seen) -> void {

			if(!dm.name.empty() && dm.name != "_")
			{
				if(seen.find(dm.name) != seen.end())
				{
					SimpleError::make(dm.loc, "duplicate binding '%s' in destructuring declaration", dm.name)
						->append(SimpleError::make(MsgType::Note, seen[dm.name], "previous binding was here:"))
						->postAndQuit();
				}
				else
				{
					seen[dm.name] = dm.loc;
				}
			}
			else if(dm.name.empty())
			{
				for(const auto& b : dm.inner)
					visit(b, seen);
			}
		};

		std::map<std::string, Location> seen;
		visit(decomp->bindings, seen);

		if(st.front() != TT::Equal)
			expected(st, "'=' for assignment to decomposition", st.front().str());

		st.pop();
		decomp->initialiser = parseExpr(st);

		return decomp;
	}

	DecompMapping parseTupleDecomp(State& st)
	{
		return recursivelyParseDecomp(st);
	}














	Stmt* parseVariable(State& st)
	{
		using TT = lexer::TokenType;

		iceAssert(st.front() == TT::Var || st.front() == TT::Val);

		bool isImmut = (st.eat() == TT::Val);
		if(st.front() == TT::LParen || st.front() == TT::LSquare)
		{
			auto ret = parseDecompDecl(st);
			ret->immut = isImmut;

			return ret;
		}
		else if(st.front() != TT::Identifier)
		{
			expectedAfter(st, "identifier", "'" + std::string(isImmut ? "val" : "var") + "'", st.front().str());
		}

		auto loc = st.front().loc;

		std::string name = st.eat().str();
		pts::Type* type = pts::InferredType::get();
		Expr* value = 0;

		if(st.front() == TT::Colon)
		{
			st.pop();
			type = parseType(st);
		}
		else if(st.front() != TT::Equal)
		{
			error(st, "expected initial value for type inference on variable '%s'", name);
		}

		if(st.front() == TT::Equal)
		{
			st.pop();
			value = parseExpr(st);
		}
		else if(isImmut)
		{
			error(st, "expected initial value for immutable variable '%s'", name);
		}

		auto ret = util::pool<VarDefn>(loc);
		ret->initialiser = value;
		ret->immut = isImmut;
		ret->type = type;
		ret->name = name;

		return ret;
	}



}
