// function.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "defs.h"
#include "pts.h"
#include "parser_internal.h"

using namespace ast;
using namespace lexer;

using TT = TokenType;
namespace parser
{
	static std::tuple<FuncDefn*, bool, Location> parseFunctionDecl(State& st)
	{
		iceAssert(st.eat() == TT::Func);
		if(st.front() != TT::Identifier)
			expectedAfter(st, "identifier", "'fn'", st.front().str());

		FuncDefn* defn = new FuncDefn(st.loc());
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

		if(st.front() != TT::LParen)
			expectedAfter(st, "'('", "function declaration to begin argument list", st.front().str());

		st.eat();
		bool isvar = false;
		Location varloc;
		while(st.front() != TT::RParen)
		{
			if(isvar)
				error(st, "Variadic arguments must be the last in the function parameter list");

			if(st.front() != TT::Identifier)
				expected(st, "identifier in function parameter list", st.front().str());

			std::string name = st.front().str();
			st.eat();

			if(st.front() != TT::Colon)
				expected(st, "':' after identifier to specify type", st.front().str());

			st.eat();
			auto type = parseType(st);

			defn->args.push_back(FuncDefn::Arg { .name = name, .type = type });

			if(st.front() == TT::Comma)
				st.eat();

			else if(st.front() != TT::RParen)
				expected(st, "')' or ',' in function parameter list", st.front().str());

			if(st.front() == TT::Ellipsis)
			{
				isvar = true;
				varloc = st.loc();
				st.pop();
			}
		}

		iceAssert(st.front() == TT::RParen);
		st.eat();

		if(st.front() == TT::Arrow)
		{
			st.eat();
			defn->returnType = parseType(st);
		}
		else
		{
			defn->returnType = pts::NamedType::create(VOID_TYPE_STRING);
		}

		return std::make_tuple(defn, isvar, varloc);
	}




	FuncDefn* parseFunction(State& st)
	{
		auto [ defn, isvar, varloc ] = parseFunctionDecl(st);
		if(isvar)
			error(st, "C-style variadic arguments are not supported on non-foreign functions");

		st.skipWS();
		if(st.front() != TT::LBrace)
			expected(st, "'{' to begin function body", st.front().str());

		defn->body = parseBracedBlock(st);
		return defn;
	}





	ast::ForeignFuncDefn* parseForeignFunction(State& st)
	{
		iceAssert(st.front() == TT::ForeignFunc);
		st.pop();

		auto ffn = new ForeignFuncDefn(st.ploc());

		// copy the things over
		auto [ defn, isvar, _ ] = parseFunctionDecl(st);
		if(!defn->generics.empty())
			error(ffn->loc, "Foreign functions cannot be generic");

		ffn->isVarArg = isvar;
		ffn->args = defn->args;
		ffn->name = defn->name;
		ffn->privacy = defn->privacy;
		ffn->returnType = defn->returnType;

		return ffn;
	}














	std::map<std::string, TypeConstraints_t> parseGenericTypeList(State& st)
	{
		std::map<std::string, TypeConstraints_t> ret;

		while(st.front().type != TT::RAngle)
		{
			if(st.front().type == TT::Identifier)
			{
				std::string gt = st.eat().text.to_string();
				TypeConstraints_t constrs;

				if(st.front().type == TT::Colon)
				{
					st.eat();
					if(st.front().type != TT::Identifier)
						error(st, "Expected identifier after beginning of type constraint list");

					while(st.front().type == TT::Identifier)
					{
						constrs.protocols.push_back(st.eat().text.to_string());

						if(st.front().type == TT::Ampersand)
						{
							st.eat();
						}
						else if(st.front().type != TT::Comma && st.front().type != TT::RAngle)
						{
							error(st, "Expected ',' or '>' to end type parameter list (1)");
						}
					}
				}
				else if(st.front().type != TT::Comma && st.front().type != TT::RAngle)
				{
					error(st, "Expected ',' or '>' to end type parameter list (2)");
				}

				ret[gt] = constrs;
			}
			else if(st.front().type == TT::Comma)
			{
				st.eat();
			}
			else if(st.front().type != TT::RAngle)
			{
				error(st, "Expected '>' to end type parameter list");
			}
		}

		iceAssert(st.eat().type == TT::RAngle);

		return ret;
	}













	Block* parseBracedBlock(State& st)
	{
		iceAssert(st.eat() == TT::LBrace);
		Block* ret = new Block(st.ploc());

		st.skipWS();
		while(st.front() != TT::RBrace)
		{
			auto stmt = parseStmt(st);
			if(auto defer = dynamic_cast<DeferredStmt*>(stmt))
				ret->deferredStatements.push_back(defer->actual);

			else
				ret->statements.push_back(stmt);

			if(st.front() == TT::NewLine || st.front() == TT::Comment || st.front() == TT::Semicolon)
				st.pop();

			else
				expected(st, "newline or semicolon to terminate a statement", st.front().str());
		}

		iceAssert(st.eat() == TT::RBrace);
		st.skipWS();

		return ret;
	}
}







