// type.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "pts.h"
#include "parser_internal.h"

using namespace ast;
using namespace lexer;

namespace parser
{
	using TT = TokenType;
	StructDefn* parseStruct(State& st)
	{
		iceAssert(st.eat() == TT::Struct);
		if(st.front() != TT::Identifier)
			expectedAfter(st, "identifier", "'struct'", st.front().str());

		StructDefn* defn = new StructDefn(st.loc());
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

		st.skipWS();
		if(st.front() != TT::LBrace)
			expectedAfter(st, "'{'", "'struct'", st.front().str());

		auto blk = parseBracedBlock(st);
		for(auto s : blk->statements)
		{
			if(auto v = dynamic_cast<VarDefn*>(s))
			{
				if(v->type == pts::InferredType::get())
					error(v, "Struct fields must have types explicitly specified");

				defn->fields.push_back(v);
			}
			else if(auto f = dynamic_cast<FuncDefn*>(s))
			{
				defn->methods.push_back(f);
			}
			else
			{
				error(s, "Unsupported expression or statement in 'struct' body");
			}
		}

		for(auto s : blk->deferredStatements)
			error(s, "Unsupported expression or statement in 'struct' body");

		return defn;
	}


















	static pts::Type* parseTypeIndirections(State& st, pts::Type* base)
	{
		using TT = TokenType;
		auto ret = base;

		while(st.front() == TT::Asterisk)
			ret = new pts::PointerType(ret), st.pop();

		if(st.front() == TT::LSquare)
		{
			// parse an array of some kind
			st.pop();

			if(st.front() == TT::RSquare)
			{
				st.eat();
				ret = new pts::DynamicArrayType(ret);
			}
			else if(st.front() == TT::Ellipsis)
			{
				st.pop();
				if(st.eat() != TT::RSquare)
					expectedAfter(st, "closing ']'", "variadic array type", st.prev().str());

				ret = new pts::VariadicArrayType(ret);
			}
			else if(st.front() == TT::Number)
			{
				long sz = std::stol(st.front().str());
				if(sz <= 0)
					expected(st, "positive, non-zero size for fixed array", st.front().str());

				st.pop();
				if(st.eat() != TT::RSquare)
					expectedAfter(st, "closing ']'", "array type", st.front().str());

				ret = new pts::FixedArrayType(ret, sz);
			}
			else if(st.front() == TT::Colon)
			{
				st.pop();
				if(st.eat() != TT::RSquare)
					expectedAfter(st, "closing ']'", "slice type", st.prev().str());

				ret = new pts::ArraySliceType(ret);
			}
			else
			{
				error(st, "Unexpected token '%s' after opening '['; expected some kind of array type",
					st.front().str().c_str());
			}
		}

		if(st.front() == TT::LSquare || st.front() == TT::Asterisk)
			return parseTypeIndirections(st, ret);

		else
			return ret;
	}

	pts::Type* parseType(State& st)
	{
		using TT = TokenType;
		if(st.front() == TT::Identifier)
		{
			std::string s = st.eat().str();

			while(st.hasTokens())
			{
				if(st.front() == TT::Period)
				{
					s += ".", st.eat();
				}
				else if(st.front() == TT::Identifier)
				{
					if(s.back() != '.')
						error(st, "Unexpected identifer '%s' in type", st.front().str().c_str());

					else
						s += st.eat().str();
				}
				else
				{
					break;
				}
			}

			auto nt = pts::NamedType::create(s);

			// check generic mapping
			if(st.front() == TT::LAngle)
			{
				// ok
				st.pop();
				while(st.hasTokens())
				{
					if(st.front() == TT::Identifier)
					{
						std::string ty = st.eat().str();
						if(st.eat() != TT::Colon)
							expected(st, "':' to specify type mapping in parametric type instantiation", st.prev().str());

						pts::Type* mapped = parseType(st);
						nt->genericMapping[ty] = mapped;


						if(st.front() == TT::Comma)
						{
							st.pop();
							continue;
						}
						else if(st.front() == TT::RAngle)
						{
							break;
						}
						else
						{
							expected(st, "either ',' or '>' to continue or terminate parametric type instantiation", st.front().str());
						}
					}
					else if(st.front() == TT::RAngle)
					{
						error(st, "Need at least one type mapping in parametric type instantiation");
					}
					else
					{
						// error(st, "Unexpected token '%s' in type mapping", st.front().str().c_str());
						break;
					}
				}

				if(st.front() != TT::RAngle)
					expected(st, "'>' to end type mapping", st.front().str());

				st.pop();
			}


			// check for indirections
			return parseTypeIndirections(st, nt);
		}
		else if(st.front() == TT::LParen)
		{
			// tuple or function
			st.pop();

			// parse a tuple.
			std::vector<pts::Type*> types;
			while(st.hasTokens() && st.front().type != TT::RParen)
			{
				// do things.
				auto ty = parseType(st);

				if(st.front() != TT::Comma && st.front() != TT::RParen)
					error(st, "Unexpected token '%s' in type specifier, expected either ',' or ')'", st.front().str().c_str());

				else if(st.front() == TT::Comma)
					st.eat();

				types.push_back(ty);
			}

			if(types.size() == 0)
				error(st, "Empty tuples '()' are not supported");

			if(st.eat().type != TT::RParen)
			{
				expected(st, "')' to end tuple type specifier", st.prev().str());
			}

			// check if it's actually a function type
			if(st.front() != TT::Arrow)
			{
				// this *should* allow us to 'group' types together
				// eg. ((i64, i64) -> i64)[] would allow us to create an array of functions
				// whereas (i64, i64) -> i64[] would parse as a function returning an array of i64s.

				if(types.size() == 1)
					return parseTypeIndirections(st, types[0]);

				auto tup = new pts::TupleType(types);
				return parseTypeIndirections(st, tup);
			}
			else
			{
				// eat the arrow, parse the type
				st.eat();
				auto rty = parseType(st);

				auto ft = new pts::FunctionType(types, rty);
				return parseTypeIndirections(st, ft);
			}
		}
		else
		{
			error(st, "Unexpected token '%s' while parsing type", st.front().str().c_str());
		}
	}
}
