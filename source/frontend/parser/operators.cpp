// operators.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "defs.h"
#include "pts.h"
#include "parser.h"
#include "parser_internal.h"

using namespace ast;
using namespace lexer;

using TT = lexer::TokenType;
namespace parser
{
	std::tuple<std::vector<FuncDefn::Arg>, std::map<std::string, TypeConstraints_t>, pts::Type*, bool, Location>
	parseFunctionLookingDecl(State& st);

	OperatorOverloadDefn* parseOperatorOverload(State& st)
	{
		using Kind = ast::OperatorOverloadDefn::Kind;

		iceAssert(st.front() == TT::Operator);
		auto ret = new ast::OperatorOverloadDefn(st.eat().loc);

		if(st.front().str() == "prefix")
			ret->kind = Kind::Prefix;

		else if(st.front().str() == "postfix")
			ret->kind = Kind::Postfix;

		else if(st.front().str() == "infix")
			ret->kind = Kind::Infix;

		else
			expectedAfter(st, "either 'infix', 'prefix' or 'postfix'", "'operator'", st.front().str());

		st.eat();
		ret->symbol = parseOperatorTokens(st);


		// check whether we can support this
		// TODO: when we actually do this, we need to check the parser state to see if it knows about this
		// TODO: *new* thing.
		//* when we do a parseUnary(), it expects a certain set of operators only. If you suddenly declare
		//* say '%' as a prefix unary operator, the parser won't know that, and will throw an error. So, if
		//* you're overloading an operator in a way that changes its parsing behaviour, you need to declare
		//* it beforehand.

		//* the parser will probably do a pre-pass, in the same pass as we do for imports, to look for operator
		//* declarations -- so we enforce that they must be at the top of the file, and are not constrained by
		//* scope.

		//* this shouldn't introduce any conflicts; we already handle unary + and - along with their binary versions,
		//* so introducting something like unary % or binary ++ or something should be perfectly fine, and we'll
		//* just use overload resolution to prevent calling the wrong thing. make sense?? probably not.

		//! needs to be actually done.


		bool isvar = false;
		std::tie(ret->args, ret->generics, ret->returnType, isvar, std::ignore) = parseFunctionLookingDecl(st);

		if(isvar) error(ret, "C-style variadic arguments are not supported on non-foreign functions");

		st.skipWS();
		if(st.front() != TT::LBrace && st.front() != TT::FatRightArrow)
			expected(st, "'{' to begin function body", st.front().str());

		ret->body = parseBracedBlock(st);
		ret->name = ret->symbol;
		return ret;
	}




	std::string parseOperatorTokens(State& st)
	{
		using TT = lexer::TokenType;

		auto tok_op = st.eat();

		if(tok_op == TT::LAngle || tok_op == TT::RAngle)
		{
			// check if the next one matches.
			if(tok_op == TT::LAngle)
			{
				if(st.front() == TT::LAngle)
				{
					// < < is <<
					st.eat();
					return "<<";
				}
				else if(st.front() == TT::LessThanEquals)
				{
					// < <= is <<=
					st.eat();
					return "<<=";
				}
			}
			else if(tok_op == TT::RAngle)
			{
				if(st.front() == TT::RAngle)
				{
					// > > is >>
					st.eat();
					return ">>";
				}
				else if(st.front() == TT::GreaterEquals)
				{
					// > >= is >>=
					st.eat();
					return ">>=";
				}
			}
		}


		switch(tok_op.type)
		{
			case TT::Plus:				return "+";
			case TT::Minus:				return "-";
			case TT::Asterisk:			return "*";
			case TT::Divide:			return "/";
			case TT::Percent:			return "%";
			case TT::ShiftLeft:			return "<<";
			case TT::ShiftRight:		return ">>";
			case TT::Equal:				return "=";

			case TT::LAngle:			return "<";
			case TT::RAngle:			return ">";
			case TT::LessThanEquals:	return "<=";
			case TT::GreaterEquals:		return ">=";
			case TT::EqualsTo:			return "==";
			case TT::NotEquals:			return "!=";

			case TT::Ampersand:			return "&";
			case TT::Pipe:				return "|";
			case TT::Caret:				return "^";
			case TT::LogicalOr:			return "||";
			case TT::LogicalAnd:		return "&&";

			case TT::PlusEq:			return "+=";
			case TT::MinusEq:			return "-=";
			case TT::MultiplyEq:		return "*=";
			case TT::DivideEq:			return "/=";
			case TT::ModEq:				return "%=";
			case TT::ShiftLeftEq:		return "<<=";
			case TT::ShiftRightEq:		return ">>=";
			case TT::AmpersandEq:		return "&=";
			case TT::PipeEq:			return "|=";
			case TT::CaretEq:			return "^=";

			case TT::Period:			return ".";
			case TT::As:				return "cast";

			default:					return "";
		}
	}

	int parseOperatorDecl(const lexer::TokenList& tokens, int idx, int* _kind, CustomOperatorDecl* out)
	{
		iceAssert(tokens[idx] == TT::Attr_Operator);
		const Token tok = tokens[idx];

		using Kind = CustomOperatorDecl::Kind;
		CustomOperatorDecl oper;
		oper.loc = tok.loc;

		idx++;

		if(tokens[idx].str() != "prefix" && tokens[idx].str() != "postfix" && tokens[idx].str() != "infix")
		{
			expectedAfter(tokens[idx].loc, "either 'prefix', 'postfix' or 'infix'", "'operator' in custom operator declaration",
				tokens[idx].str());
		}

		int kind = 0;
		if(tokens[idx].str() == "infix")		kind = 1, oper.kind = Kind::Infix;
		else if(tokens[idx].str() == "prefix")	kind = 2, oper.kind = Kind::Prefix;
		else if(tokens[idx].str() == "postfix")	kind = 3, oper.kind = Kind::Postfix;
		else									iceAssert(0);

		idx++;

		{
			if(tokens[idx] != TT::Number)
				expectedAfter(tokens[idx].loc, "integer value", "to specify precedence value", tokens[idx].str());

			// make sure it's an integer.
			auto num = tokens[idx].str();
			if(num.find('.') != std::string::npos)
				expected(tokens[idx].loc, "integer value for precedence", num);

			int prec = std::stoi(num);
			if(prec <= 0)
				expected(tokens[idx].loc, "a precedence value greater than zero", num);

			oper.precedence = prec;
			idx++;
		}


		oper.symbol = tokens[idx].str();
		idx++;



		if(tokens[idx] != TT::NewLine && tokens[idx] != TT::Semicolon && tokens[idx] != TT::Comment)
		{
			error(tokens[idx].loc, "Expected newline or semicolon to terminate operator declaration, found '%s'", tokens[idx].str());
		}

		if(_kind)	*_kind = kind;
		if(out)		*out = oper;

		return idx;
	}


	std::tuple<std::unordered_map<std::string, parser::CustomOperatorDecl>,
				std::unordered_map<std::string, parser::CustomOperatorDecl>,
				std::unordered_map<std::string, parser::CustomOperatorDecl>>
	parseOperators(const lexer::TokenList& tokens)
	{
		using Token = lexer::Token;
		using TT = lexer::TokenType;

		std::unordered_map<std::string, CustomOperatorDecl> infix;
		std::unordered_map<std::string, CustomOperatorDecl> prefix;
		std::unordered_map<std::string, CustomOperatorDecl> postfix;

		// basically, this is how it goes:
		// only allow comments to occur before imports
		// all imports must happen before anything else in the file
		// comments can be interspersed between import statements, of course.
		for(size_t i = 0; i < tokens.size(); i++)
		{
			const Token& tok = tokens[i];
			if(tok == TT::Attr_Operator)
			{
				CustomOperatorDecl oper;
				int kind = 0;

				i = parseOperatorDecl(tokens, i, &kind, &oper);

				if(kind == 1)		infix[oper.symbol] = oper;
				else if(kind == 2)	prefix[oper.symbol] = oper;
				else if(kind == 3)	postfix[oper.symbol] = oper;
			}
			else if(tok == TT::Export)
			{
				// skip the name as well
				i++;
			}
			else if(tok == TT::Import)
			{
				// skip until a newline.
				while(tokens[i] != TT::Comment && tokens[i] != TT::NewLine)
					i++;
			}
			else if(tok == TT::Comment || tok == TT::NewLine)
			{
				// skipped
			}
			else
			{
				// stop imports.
				break;
			}
		}

		return std::make_tuple(infix, prefix, postfix);
	}
}



























