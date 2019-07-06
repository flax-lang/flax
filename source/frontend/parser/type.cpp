// type.cpp
// Copyright (c) 2014 - 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include "pts.h"
#include "parser_internal.h"

#include "mpool.h"

using namespace ast;
using namespace lexer;

namespace parser
{
	using TT = lexer::TokenType;

	template <typename T>
	static void addSelfToMethod(T* thing, bool mutating)
	{
		// insert the self argument in the front.
		FuncDefn::Param self;
		self.name = "this";
		self.loc = thing->loc;
		self.type = util::pool<pts::PointerType>(self.loc, pts::NamedType::create(self.loc, "self"), mutating);

		thing->params.insert(thing->params.begin(), self);
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

				v->isField = true;
				defn->fields.push_back(v);
			}
			else if(auto f = dcast(FuncDefn, s))
			{
				addSelfToMethod(f, f->isMutating);
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
				addSelfToMethod(init, /* mutating: */ true);

				if(init->name == "init")
					defn->initialisers.push_back(init);

				else if(init->name == "deinit")
					defn->deinitialiser = init;

				else if(init->name == "copy")
					defn->copyInitialiser = init;

				else
					error(s, "wtf? '%s'", init->name);
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











	StructDefn* parseStruct(State& st, bool nameless)
	{
		static size_t anon_counter = 0;

		iceAssert(st.front() == TT::Struct);
		st.eat();

		StructDefn* defn = util::pool<StructDefn>(st.loc());
		if(nameless)
		{
			defn->name = util::obfuscateName("anon_struct", anon_counter++);
		}
		else
		{
			if(st.front() == TT::LBrace)
				error(st, "declared structs (in non-type usage) must be named");

			else if(st.front() != TT::Identifier)
				expectedAfter(st, "identifier", "'struct'", st.front().str());

			else
				defn->name = st.eat().str();
		}


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
			expectedAfter(st.ploc(), "opening brace", "'struct'", st.front().str());

		st.enterStructBody();
		st.skipWS();

		size_t index = 0;
		while(st.front() != TT::RBrace)
		{
			st.skipWS();

			if(st.front() == TT::Identifier)
			{
				auto loc = st.loc();
				std::string name = st.eat().str();

				// we can't check for duplicates when it's transparent, duh
				// we'll collapse and collect and check during typechecking.
				if(name != "_")
				{
					if(auto it = std::find_if(defn->fields.begin(), defn->fields.end(), [&name](const auto& p) -> bool {
						return std::get<0>(p) == name;
					}); it != defn->fields.end())
					{
						SimpleError::make(loc, "duplicate field '%s' in struct definition", name)
							->append(SimpleError::make(MsgType::Note, std::get<1>(*it), "field '%s' previously defined here:", name))
							->postAndQuit();
					}
				}

				if(st.eat() != TT::Colon)
					error(st.ploc(), "expected type specifier after field name in struct");

				pts::Type* type = parseType(st);
				defn->fields.push_back(std::make_tuple(name, loc, type));

				if(st.front() == TT::Equal)
					error(st.loc(), "struct fields cannot have initialisers");
			}
			else if(st.front() == TT::Func)
			{
				// ok parse a func as usual
				auto method = parseFunction(st);
				addSelfToMethod(method, method->isMutating);

				defn->methods.push_back(method);
			}
			else if(st.front() == TT::Var || st.front() == TT::Val)
			{
				error(st.loc(), "struct fields are declared as 'name: type'; val/let is omitted");
			}
			else if(st.front() == TT::Static)
			{
				error(st.loc(), "structs cannot have static declarations");
			}
			else if(st.front() == TT::NewLine || st.front() == TT::Semicolon)
			{
				st.pop();
			}
			else if(st.front() == TT::RBrace)
			{
				break;
			}
			else
			{
				error(st.loc(), "unexpected token '%s' inside struct body", st.front().str());
			}

			index++;
		}

		iceAssert(st.front() == TT::RBrace);
		st.eat();

		st.leaveStructBody();
		return defn;
	}


	UnionDefn* parseUnion(State& st, bool israw, bool nameless)
	{
		static size_t anon_counter = 0;
		iceAssert(st.front() == TT::Union);
		st.eat();

		UnionDefn* defn = util::pool<UnionDefn>(st.loc());
		if(nameless)
		{
			defn->name = util::obfuscateName("anon_union", anon_counter++);
		}
		else
		{
			if(st.front() == TT::LBrace)
				error(st, "declared unions (in non-type usage) must be named");

			else if(st.front() != TT::Identifier)
				expectedAfter(st, "identifier", "'union'", st.front().str());

			else
				defn->name = st.eat().str();
		}


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

		st.skipWS();
		if(st.front() == TT::RBrace)
			error(st, "union must contain at least one variant");

		size_t index = 0;
		while(st.front() != TT::RBrace)
		{
			st.skipWS();

			if(st.front() != TT::Identifier)
			{
				if(st.front() == TT::Var || st.front() == TT::Val)
					error(st.loc(), "union fields are declared as 'name: type'; val/let is omitted");

				else
					expected(st.loc(), "identifier inside union body", st.front().str());
			}

			auto loc = st.loc();
			pts::Type* type = 0;
			std::string name = st.eat().str();

			// to improve code flow, handle the type first.
			if(st.front() == TT::Colon)
			{
				st.eat();
				type = parseType(st);
			}
			else
			{
				if(israw)
					error(st.loc(), "raw unions cannot have empty variants (must have a type)");

				else if(st.front() != TT::NewLine)
					error(st.loc(), "expected newline after union variant");
			}

			if(name == "_")
			{
				if(!israw)
					error(loc, "transparent fields can only be present in raw unions");

				iceAssert(type);
				defn->transparentFields.push_back({ loc, type });
			}
			else
			{
				if(auto it = defn->cases.find(name); it != defn->cases.end())
				{
					SimpleError::make(loc, "duplicate variant '%s' in union definition", name)
						->append(SimpleError::make(MsgType::Note, std::get<1>(it->second), "variant '%s' previously defined here:", name))
						->postAndQuit();
				}

				if(type == nullptr) type = pts::NamedType::create(loc, VOID_TYPE_STRING);
				defn->cases[name] = { index, loc, type };
			}

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

		defn->israw = israw;
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
				//? this is mostly because we don't want to deal with auto-incrementing stuff
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
			auto l = st.loc();
			st.pop();

			// check for mutability.
			bool mut = st.front() == TT::Mutable;
			if(mut) st.pop();

			auto base = parseType(st);
			l.len = base->loc.col - l.col + base->loc.len;

			return util::pool<pts::PointerType>(l, base, mut);
		}
		else if(st.front() == TT::LogicalAnd)
		{
			// lmao.
			auto l = st.loc();
			st.pop();

			bool mut = st.front() == TT::Mutable;
			if(mut) st.pop();

			//* note: this handles cases like & (&mut T)
			//* so, the outer pointer is never mutable, but the inner one might be.

			auto base = parseType(st);
			l.len = base->loc.col - l.col + base->loc.len;

			return util::pool<pts::PointerType>(l, util::pool<pts::PointerType>(l, base, mut), false);
		}
		else if(st.front() == TT::LSquare)
		{
			// [T] is a dynamic array
			// [T:] is a slice
			// [T: N] is a fixed array of size 'N'

			auto loc = st.loc();

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
					loc.len = st.loc().col - loc.col + st.loc().len;
					st.pop();
					return util::pool<pts::ArraySliceType>(loc, elm, mut);
				}
				else if(st.front() == TT::Ellipsis)
				{
					st.pop();
					loc.len = st.loc().col - loc.col + st.loc().len;

					if(st.pop() != TT::RSquare)
						expectedAfter(st, "']'", "... in variadic array type", st.front().str());

					return util::pool<pts::VariadicArrayType>(loc, elm);
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
					loc.len = st.loc().col - loc.col + st.loc().len;

					if(st.eat() != TT::RSquare)
						expectedAfter(st, "closing ']'", "array type", st.front().str());

					//! ACHTUNG !
					// TODO: support mutable arrays??
					return util::pool<pts::FixedArrayType>(loc, elm, sz);
				}
			}
			else if(st.front() == TT::RSquare)
			{
				// dynamic array.
				if(mut) error(st.loc(), "dynamic arrays are always mutable, specifying 'mut' is unnecessary");

				loc.len = st.loc().col - loc.col + st.loc().len;

				st.pop();
				return util::pool<pts::DynamicArrayType>(loc, elm);
			}
			else
			{
				expected(st.loc(), "']' in array type specifier", st.front().str());
			}
		}
		else if(st.front() == TT::Identifier || st.front() == TT::DoubleColon || st.front() == TT::Caret)
		{
			if(st.front() == TT::Caret && st.lookahead(1) != TT::DoubleColon)
				error(st, "'^' in type specifier must be followed by '::' (to specify parent scope)");

			auto loc = st.loc();
			std::string s = st.eat().str();

			while(st.hasTokens())
			{
				if(st.front() == TT::DoubleColon)
				{
					s += st.eat().str();
				}
				else if(st.front() == TT::Caret)
				{
					s += st.eat().str();
					if(st.front() != TT::DoubleColon)
						error(st, "expected '::' after '^' in scope path");
				}
				else if(st.front() == TT::Identifier)
				{
					if(s.back() != ':')
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
			PolyArgMapping_t pams;
			if(st.front() == TT::Exclamation && st.lookahead(1) == TT::LAngle)
			{
				//? note: if *failed is nullptr, then we will throw errors where we usually would return.
				pams = parsePAMs(st, /* fail: */ nullptr);
			}

			loc.len = st.ploc().col - loc.col + st.ploc().len;
			return pts::NamedType::create(loc, s, pams);
		}
		else if(auto isfn = (st.front() == TT::Func); st.front() == TT::LParen || st.front() == TT::Func)
		{
			// tuple or function
			auto loc = st.loc();

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

			loc.len = st.loc().col - loc.col + st.loc().len;
			if(st.eat().type != TT::RParen)
				expected(st, "')' to end type list", st.prev().str());


			if(isfn)
			{
				if(st.front() != TT::RightArrow)
					expected(st, "'->' in function type specifier after parameter types", st.front().str());

				st.eat();

				// eat the arrow, parse the type
				auto retty = parseType(st);

				loc.len = st.ploc().col - loc.col + st.ploc().len;

				return util::pool<pts::FunctionType>(loc, types, retty);
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

				return util::pool<pts::TupleType>(loc, types);
			}
		}
		else if(st.front() == TT::Struct)
		{
			auto str = parseStruct(st, /* nameless: */ true);
			st.anonymousTypeDefns.push_back(str);

			return pts::NamedType::create(str->loc, str->name);
		}
		else if(st.front() == TT::Union || (st.front() == TT::Attr_Raw && st.lookahead(1) == TT::Union))
		{
			bool israw = st.front() == TT::Attr_Raw;
			if(israw) st.eat();

			auto unn = parseUnion(st, israw, /* nameless: */ true);
			st.anonymousTypeDefns.push_back(unn);

			return pts::NamedType::create(unn->loc, unn->name);
		}
		else if(st.front() == TT::Class)
		{
			error(st, "classes cannot be defined anonymously");
		}
		else
		{
			error(st, "unexpected token '%s' while parsing type", st.front().str());
		}
	}


	// PAM == PolyArgMapping
	PolyArgMapping_t parsePAMs(State& st, bool* failed)
	{
		iceAssert(st.front() == TT::Exclamation && st.lookahead(1) == TT::LAngle);
		auto openLoc = st.loc();

		st.pop();
		st.pop();

		std::unordered_set<std::string> seen;
		PolyArgMapping_t mappings;
		{
			//* foo!<> is an error regardless of whether we're doing expression parsing or call parsing.
			if(st.front() == TT::RAngle)
				error(Location::unionOf(openLoc, st.loc()), "type parameter list cannot be empty");

			// step 2A: start parsing.
			size_t idx = 0;
			while(st.front() != TT::RAngle)
			{
				if(st.front() != TT::Identifier)
				{
					if(failed)
					{
						*failed = true;
						return mappings;
					}
					else
					{
						expected(st.loc(), "identifier in type argument list", st.front().str());
					}
				}

				if(st.front() == TT::Identifier && st.lookahead(1) == TT::Colon)
				{
					auto id = st.pop().str();
					st.pop();

					//? I think beyond this point we pretty much can't fail since we have the colon.
					//? so, we shouldn't need to handle the case where we fail to parse a type here.
					if(seen.find(id) != seen.end())
						error(st.loc(), "duplicate type argument '%s'", id);

					auto ty = parseType(st);
					mappings.add(id, ty);
					seen.insert(id);
				}
				else
				{
					if(!seen.empty())
						error(st.loc(), "cannot have positional type arguments after named arguments");

					auto ty = parseType(st);
					mappings.add(idx++, ty);
				}

				if(st.front() == TT::Comma)
					st.pop();

				else if(st.front() != TT::RAngle)
					expected(st.loc(), "',' or '>' in type argument list", st.front().str());
			}

			iceAssert(st.front() == TT::RAngle);
			st.pop();

			return mappings;
		}
	}
}













