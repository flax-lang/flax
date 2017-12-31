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
	std::tuple<std::vector<FuncDefn::Arg>, std::map<std::string, TypeConstraints_t>, pts::Type*, bool, Location> parseFunctionLookingDecl(State& st);

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
		ret->op = parseOperatorTokens(st);


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


		if(ret->op == Operator::Invalid)
			error(st.ploc(), "Invalid operator '%s' (custom operators are not yet supported)", st.prev().str());

		bool isvar = false;
		std::tie(ret->args, ret->generics, ret->returnType, isvar, std::ignore) = parseFunctionLookingDecl(st);

		if(isvar) error(ret, "C-style variadic arguments are not supported on non-foreign functions");

		st.skipWS();
		if(st.front() != TT::LBrace && st.front() != TT::FatRightArrow)
			expected(st, "'{' to begin function body", st.front().str());

		ret->body = parseBracedBlock(st);
		ret->name = operatorToString(ret->op);
		return ret;
	}




	Operator parseOperatorTokens(State& st)
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
					return Operator::ShiftLeft;
				}
				else if(st.front() == TT::LessThanEquals)
				{
					// < <= is <<=
					st.eat();
					return Operator::ShiftLeftEquals;
				}
			}
			else if(tok_op == TT::RAngle)
			{
				if(st.front() == TT::RAngle)
				{
					// > > is >>
					st.eat();
					return Operator::ShiftRight;
				}
				else if(st.front() == TT::GreaterEquals)
				{
					// > >= is >>=
					st.eat();
					return Operator::ShiftRightEquals;
				}
			}
		}


		switch(tok_op.type)
		{
			case TT::Plus:				return Operator::Add;
			case TT::Minus:				return Operator::Subtract;
			case TT::Asterisk:			return Operator::Multiply;
			case TT::Divide:			return Operator::Divide;
			case TT::Percent:			return Operator::Modulo;
			case TT::ShiftLeft:			return Operator::ShiftLeft;
			case TT::ShiftRight:		return Operator::ShiftRight;
			case TT::Equal:				return Operator::Assign;

			case TT::LAngle:			return Operator::CompareLess;
			case TT::RAngle:			return Operator::CompareGreater;
			case TT::LessThanEquals:	return Operator::CompareLessEq;
			case TT::GreaterEquals:		return Operator::CompareGreaterEq;
			case TT::EqualsTo:			return Operator::CompareEq;
			case TT::NotEquals:			return Operator::CompareNotEq;

			case TT::Ampersand:			return Operator::BitwiseAnd;
			case TT::Pipe:				return Operator::BitwiseOr;
			case TT::Caret:				return Operator::BitwiseXor;
			case TT::LogicalOr:			return Operator::LogicalOr;
			case TT::LogicalAnd:		return Operator::LogicalAnd;

			case TT::PlusEq:			return Operator::PlusEquals;
			case TT::MinusEq:			return Operator::MinusEquals;
			case TT::MultiplyEq:		return Operator::MultiplyEquals;
			case TT::DivideEq:			return Operator::DivideEquals;
			case TT::ModEq:				return Operator::ModuloEquals;
			case TT::ShiftLeftEq:		return Operator::ShiftLeftEquals;
			case TT::ShiftRightEq:		return Operator::ShiftRightEquals;
			case TT::AmpersandEq:		return Operator::BitwiseAndEquals;
			case TT::PipeEq:			return Operator::BitwiseOrEquals;
			case TT::CaretEq:			return Operator::BitwiseXorEquals;

			case TT::Period:			return Operator::DotOperator;
			case TT::As:				return Operator::Cast;

			default:					return Operator::Invalid;
		}
	}


	std::tuple<std::vector<CustomOperatorDecl>, std::vector<CustomOperatorDecl>, std::vector<CustomOperatorDecl>>
	parseOperators(const std::string& filename, const lexer::TokenList& tokens)
	{
		using Token = lexer::Token;
		using TT = lexer::TokenType;

		std::vector<std::vector<CustomOperatorDecl>> infix;
		std::vector<std::vector<CustomOperatorDecl>> prefix;
		std::vector<std::vector<CustomOperatorDecl>> postfix;

		// basically, this is how it goes:
		// only allow comments to occur before imports
		// all imports must happen before anything else in the file
		// comments can be interspersed between import statements, of course.
		for(size_t i = 0; i < tokens.size(); i++)
		{
			const Token& tok = tokens[i];
			if(tok.str() == "prefix" || tok.str() == "postfix")
			{
				i++;

				Location loc = tok.loc;
				std::string op;

				if(tokens[i] != TT::Operator)
					expectedAfter(tokens[i].loc, "'operator'", "'" + tok.str() + "' in custom operator declaration", tokens[i].str());

				i++;
				op = tokens[i].str();
				i++;

				if(tokens[i] != TT::NewLine && tokens[i] != TT::Semicolon && tokens[i] != TT::Comment)
				{
					error(tokens[i].loc, "Expected newline or semicolon to terminate operator declaration, found '%s'", tokens[i].str());
				}

				CustomOperatorDecl opr;
				opr.kind = (tok.str() == "prefix" ? CustomOperatorDecl::Kind::Prefix : CustomOperatorDecl::Kind::Postfix);



				if(tok.str() == "prefix")	prefix.push_back(
				// i++ handled by loop
			}
			else if(tok == TT::Export)
			{
				// skip the name as well
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

		return imports;
	}
}



























