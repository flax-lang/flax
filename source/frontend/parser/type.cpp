// type.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "pts.h"
#include "parser_internal.h"

#include "mpool.h"

using namespace ast;
using namespace lexer;

namespace parser
{
	using TT = lexer::TokenType;
	StructDefn* parseStruct(State& st)
	{
		iceAssert(st.front() == TT::Struct);
		st.eat();

		if(st.front() != TT::Identifier)
			expectedAfter(st, "identifier", "'struct'", st.front().str());

		StructDefn* defn = util::pool<StructDefn>(st.loc());
		defn->name = st.eat().str();

		// check for generic function
		if(st.front() == TT::LAngle)
		{
			st.eat();
			// parse generic
			if(st.front() == TT::RAngle)
				error(st, "empty type parameter lists are not allowed");

			defn->generics = parseGenericTypeList(st);
		}

		st.skipWS();
		if(st.front() != TT::LBrace)
			expectedAfter(st, "'{'", "'struct'", st.front().str());

		st.enterStructBody();

		auto blk = parseBracedBlock(st);
		for(auto s : blk->statements)
		{
			if(auto v = dcast(VarDefn, s))
			{
				if(v->type == pts::InferredType::get())
					error(v, "struct fields must have types explicitly specified");

				else if(v->initialiser)
					error(v->initialiser, "struct fields cannot have inline initialisers");

				defn->fields.push_back(v);
			}
			else if(auto f = dcast(FuncDefn, s))
			{
				defn->methods.push_back(f);
			}
			else if(auto t = dcast(TypeDefn, s))
			{
				defn->nestedTypes.push_back(t);
			}
			else if(dcast(InitFunctionDefn, s))
			{
				error(s, "structs cannot have user-defined initialisers");
			}
			else
			{
				error(s, "unsupported expression or statement in struct body");
			}
		}

		for(auto s : blk->deferredStatements)
			error(s, "unsupported expression or statement in struct body");

		st.leaveStructBody();
		return defn;
	}


	ClassDefn* parseClass(State& st)
	{
		iceAssert(st.front() == TT::Class);
		st.eat();

		if(st.front() != TT::Identifier)
			expectedAfter(st, "identifier", "'struct'", st.front().str());

		ClassDefn* defn = util::pool<ClassDefn>(st.loc());
		defn->name = st.eat().str();

		// check for generic function
		if(st.front() == TT::LAngle)
		{
			st.eat();
			// parse generic
			if(st.front() == TT::RAngle)
				error(st, "empty type parameter lists are not allowed");

			defn->generics = parseGenericTypeList(st);
		}

		st.skipWS();
		if(st.front() == TT::Colon)
		{
			// the inheritance list.
			st.eat();

			while(true)
			{
				defn->bases.push_back(parseType(st));
				if(st.front() == TT::Comma)
				{
					st.pop();
					continue;
				}
				else
				{
					break;
				}
			}
		}

		st.skipWS();
		if(st.front() != TT::LBrace)
			expectedAfter(st, "'{'", "'class'", st.front().str());

		st.enterStructBody();

		auto blk = parseBracedBlock(st);
		for(auto s : blk->statements)
		{
			if(auto v = dcast(VarDefn, s))
			{
				if(v->type == pts::InferredType::get())
					error(v, "class fields must have types explicitly specified");

				defn->fields.push_back(v);
			}
			else if(auto f = dcast(FuncDefn, s))
			{
				defn->methods.push_back(f);
			}
			else if(auto t = dcast(TypeDefn, s))
			{
				defn->nestedTypes.push_back(t);
			}
			else if(auto st = dcast(StaticDecl, s))
			{
				if(auto fn = dcast(FuncDefn, st->actual))
					defn->staticMethods.push_back(fn);

				else if(auto vr = dcast(VarDefn, st->actual))
					defn->staticFields.push_back(vr);

				else
					error(st, "unsupported static statement in class body");
			}
			else if(auto init = dcast(InitFunctionDefn, s))
			{
				defn->initialisers.push_back(init);
			}
			else
			{
				error(s, "unsupported expression or statement in class body");
			}
		}

		for(auto s : blk->deferredStatements)
			error(s, "unsupported expression or statement in class body");

		st.leaveStructBody();
		return defn;
	}












	UnionDefn* parseUnion(State& st)
	{
		iceAssert(st.front() == TT::Union);
		st.eat();

		if(st.front() != TT::Identifier)
			expectedAfter(st, "identifier", "'union'", st.front().str());

		UnionDefn* defn = util::pool<UnionDefn>(st.loc());
		defn->name = st.eat().str();

		// check for generic function
		if(st.front() == TT::LAngle)
		{
			st.eat();
			// parse generic
			if(st.front() == TT::RAngle)
				error(st, "empty type parameter lists are not allowed");

			defn->generics = parseGenericTypeList(st);
		}

		// unions don't inherit stuff (for now????) so we don't check for it.

		st.skipWS();
		if(st.eat() != TT::LBrace)
			expectedAfter(st.ploc(), "opening brace", "'union'", st.front().str());

		size_t index = 0;
		while(st.front() != TT::RBrace)
		{
			st.skipWS();

			if(st.front() != TT::Identifier)
				expected(st.loc(), "identifier inside union body", st.front().str());

			auto loc = st.loc();
			pts::Type* type = 0;
			std::string name = st.eat().str();

			if(auto it = defn->cases.find(name); it != defn->cases.end())
			{
				SimpleError::make(loc, "duplicate variant '%s' in union definition", name)
					->append(SimpleError::make(MsgType::Note, std::get<1>(it->second), "variant '%s' previously defined here:", name))
					->postAndQuit();
			}

			if(st.front() == TT::Colon)
			{
				st.eat();
				type = parseType(st);
			}
			else if(st.front() != TT::NewLine)
			{
				error(st.loc(), "expected newline after union variant");
			}

			defn->cases[name] = { index, loc, type };

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
				error(st.loc(), "unexpected token '%s' inside union body", st.front().str());
			}

			index++;
		}

		iceAssert(st.front() == TT::RBrace);
		st.eat();

		return defn;
	}




	EnumDefn* parseEnum(State& st)
	{
		iceAssert(st.front() == TT::Enum);
		st.eat();

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
					error(st.loc(), "enumeration member type must be specified when assigning explicit values to cases");

				// ok, parse a value
				st.eat();
				value = parseExpr(st);

				hadValue = true;
			}
			else if(hadValue)
			{
				// todo: remove this restriction maybe
				error(st.loc(), "enumeration cases must either all have no values, or all have values; a mix is not allowed.");
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
				error(st.loc(), "unexpected token '%s' inside enum body", st.front().str());
			}
		}

		iceAssert(st.front() == TT::RBrace);
		st.eat();

		auto ret = util::pool<EnumDefn>(idloc);
		ret->name = name;
		ret->cases = cases;
		ret->memberType = memberType;


		return ret;
	}







	StaticDecl* parseStaticDecl(State& st)
	{
		iceAssert(st.front() == TT::Static);
		st.eat();

		auto stmt = parseStmt(st);
		if(dcast(FuncDefn, stmt) || dcast(VarDefn, stmt))
			return util::pool<StaticDecl>(stmt);

		else
			error(stmt, "'static' can only be used on function and field definitions inside class bodies");
	}





	pts::Type* parseType(State& st)
	{
		using TT = lexer::TokenType;
		if(st.front() == TT::Ampersand)
		{
			st.pop();

			// check for mutability.
			bool mut = st.front() == TT::Mutable;
			if(mut) st.pop();

			return util::pool<pts::PointerType>(parseType(st), mut);
		}
		else if(st.front() == TT::LogicalAnd)
		{
			// lmao.

			st.pop();
			bool mut = st.front() == TT::Mutable;
			if(mut) st.pop();

			//* note: above handles cases like & (&mut T)
			//* so, the outer pointer is never mutable, but the inner one might be.
			return util::pool<pts::PointerType>(util::pool<pts::PointerType>(parseType(st), mut), false);
		}
		else if(st.front() == TT::LSquare)
		{
			// [T] is a dynamic array
			// [T:] is a slice
			// [T: N] is a fixed array of size 'N'

			st.pop();

			bool mut = false;
			if(st.front() == TT::Mutable)
				mut = true, st.pop();

			auto elm = parseType(st);

			if(st.front() == TT::Colon)
			{
				st.pop();
				if(st.front() == TT::RSquare)
				{
					st.pop();
					return util::pool<pts::ArraySliceType>(elm, mut);
				}
				else if(st.front() == TT::Ellipsis)
				{
					st.pop();
					if(st.pop() != TT::RSquare)
						expectedAfter(st, "']'", "... in variadic array type", st.front().str());

					return util::pool<pts::VariadicArrayType>(elm);
				}
				else if(st.front() != TT::Number)
				{
					expected(st, "positive, non-zero size for fixed array", st.front().str());
				}
				else
				{
					long sz = std::stol(st.front().str());
					if(sz <= 0)
						expected(st, "positive, non-zero size for fixed array", st.front().str());

					st.pop();
					if(st.eat() != TT::RSquare)
						expectedAfter(st, "closing ']'", "array type", st.front().str());

					//! ACHTUNG !
					// TODO: support mutable arrays??
					return util::pool<pts::FixedArrayType>(elm, sz);
				}
			}
			else if(st.front() == TT::RSquare)
			{
				// dynamic array.
				if(mut) error(st.loc(), "dynamic arrays are always mutable, specifying 'mut' is unnecessary");

				st.pop();
				return util::pool<pts::DynamicArrayType>(elm);
			}
			else
			{
				expected(st.loc(), "']' in array type specifier", st.front().str());
			}
		}
		else if(st.front() == TT::Identifier)
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
						error(st, "unexpected identifer '%s' in type", st.front().str());

					else
						s += st.eat().str();
				}
				else
				{
					break;
				}
			}


			// check generic mapping
			std::map<std::string, pts::Type*> gmaps;
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

						if(gmaps.find(ty) != gmaps.end())
							error(st, "duplicate mapping for parameter '%s' in type arguments to parametric type '%s'", ty, s);

						gmaps[ty] = parseType(st);

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
						error(st, "need at least one type mapping in parametric type instantiation");
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


			return pts::NamedType::create(s, gmaps);
		}
		else if(auto isfn = (st.front() == TT::Func); st.front() == TT::LParen || st.front() == TT::Func)
		{
			// tuple or function
			st.pop();

			if(isfn) st.pop();

			// parse a tuple.
			std::vector<pts::Type*> types;
			while(st.hasTokens() && st.front().type != TT::RParen)
			{
				// do things.
				auto ty = parseType(st);

				if(st.front() != TT::Comma && st.front() != TT::RParen)
					error(st, "unexpected token '%s' in type specifier, expected either ',' or ')'", st.front().str());

				else if(st.front() == TT::Comma)
					st.eat();

				types.push_back(ty);
			}

			if(st.eat().type != TT::RParen)
				expected(st, "')' to end type list", st.prev().str());


			if(isfn)
			{
				if(st.front() != TT::RightArrow)
					expected(st, "'->' in function type specifier after parameter types", st.front().str());

				st.eat();
				// eat the arrow, parse the type
				return util::pool<pts::FunctionType>(types, parseType(st));
			}
			else
			{
				// this *should* allow us to 'group' types together
				// eg. ((i64, i64) -> i64)[] would allow us to create an array of functions
				// whereas (i64, i64) -> i64[] would parse as a function returning an array of i64s.

				if(types.size() == 0)
					error(st, "empty tuples '()' are not supported");

				else if(types.size() == 1)
					return types[0];

				return util::pool<pts::TupleType>(types);
			}
		}
		else
		{
			error(st, "unexpected token '%s' while parsing type", st.front().str());
		}
	}
}
