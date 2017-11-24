// type.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "pts.h"
#include "parser_internal.h"

using namespace ast;
using namespace lexer;

namespace parser
{
	using TT = lexer::TokenType;
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
			else if(auto t = dynamic_cast<TypeDefn*>(s))
			{
				defn->nestedTypes.push_back(t);
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



	ClassDefn* parseClass(State& st)
	{
		iceAssert(st.eat() == TT::Class);
		if(st.front() != TT::Identifier)
			expectedAfter(st, "identifier", "'struct'", st.front().str());

		ClassDefn* defn = new ClassDefn(st.loc());
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
			expectedAfter(st, "'{'", "'class'", st.front().str());

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
			else if(auto t = dynamic_cast<TypeDefn*>(s))
			{
				defn->nestedTypes.push_back(t);
			}
			else if(auto st = dynamic_cast<StaticStmt*>(s))
			{
				if(auto fn = dynamic_cast<FuncDefn*>(st->actual))
					defn->staticMethods.push_back(fn);

				else if(auto vr = dynamic_cast<VarDefn*>(st->actual))
					defn->staticFields.push_back(vr);

				else
					error(st, "Unsupported static statement in 'class' body");
			}
			else
			{
				error(s, "Unsupported expression or statement in 'class' body");
			}
		}

		for(auto s : blk->deferredStatements)
			error(s, "Unsupported expression or statement in 'class' body");

		return defn;
	}


	EnumDefn* parseEnum(State& st)
	{
		iceAssert(st.eat() == TT::Enum);
		if(st.front() != TT::Identifier)
			expectedAfter(st, "identifier", "'enum'", st.front().str());

		auto idloc = st.loc();
		std::string name = st.eat().str();
		pts::Type* memberType = 0;

		if(st.front() == TT::Colon)
		{
			st.eat();
			memberType = parseType(st);
		}

		// ok...
		st.skipWS();
		if(st.eat() != TT::LBrace)
			expectedAfter(st.ploc(), "opening brace", "'enum'", st.front().str());

		bool hadValue = false;
		std::vector<EnumDefn::Case> cases;
		while(st.front() != TT::RBrace)
		{
			st.skipWS();

			if(st.eat() != TT::Case)
				expected(st.ploc(), "'case' inside enum body", st.prev().str());

			if(st.front() != TT::Identifier)
				expectedAfter(st.loc(), "identifier", "'case' in enum body", st.front().str());

			std::string cn = st.eat().str();
			Expr* value = 0;

			if(st.frontAfterWS() == TT::Equal)
			{
				if(memberType == 0)
					error(st.loc(), "Enumeration member type must be specified when assigning explicit values to cases");

				// ok, parse a value
				st.eat();
				value = parseExpr(st);

				hadValue = true;
			}
			else if(hadValue)
			{
				// todo: remove this restriction maybe
				error(st.loc(), "Enumeration cases must either all have no values, or all have values; a mix is not allowed.");
			}

			// ok.
			cases.push_back(EnumDefn::Case { st.loc(), cn, value });

			// do some things
			if(st.front() == TT::NewLine || st.front() == TT::Semicolon)
			{
				st.pop();
			}
			else if(st.front() == TT::RBrace)
			{
				break;
			}
			else
			{
				error(st.loc(), "Unexpected token '%s' inside enum body", st.front().str());
			}
		}

		iceAssert(st.front() == TT::RBrace);
		st.eat();

		auto ret = new EnumDefn(idloc);
		ret->name = name;
		ret->cases = cases;
		ret->memberType = memberType;


		return ret;
	}







	StaticStmt* parseStaticStmt(State& st)
	{
		iceAssert(st.front() == TT::Static);
		st.eat();

		auto stmt = parseStmt(st);
		if(dynamic_cast<FuncDefn*>(stmt) || dynamic_cast<VarDefn*>(stmt))
			return new StaticStmt(stmt);

		else
			error(stmt, "'static' can only be used on function and field definitions inside class bodies");
	}







	static pts::Type* parseTypeIndirections(State& st, pts::Type* base)
	{
		using TT = lexer::TokenType;
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
					st.front().str());
			}
		}

		if(st.front() == TT::LSquare || st.front() == TT::Asterisk)
			return parseTypeIndirections(st, ret);

		else
			return ret;
	}

	pts::Type* parseType(State& st)
	{
		using TT = lexer::TokenType;
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
						error(st, "Unexpected identifer '%s' in type", st.front().str());

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
						// error(st, "Unexpected token '%s' in type mapping", st.front().str());
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
					error(st, "Unexpected token '%s' in type specifier, expected either ',' or ')'", st.front().str());

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
			error(st, "Unexpected token '%s' while parsing type", st.front().str());
		}
	}
}
