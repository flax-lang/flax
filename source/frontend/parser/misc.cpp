// misc.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "frontend.h"
#include "parser_internal.h"

using namespace ast;
using namespace lexer;

namespace parser
{
	ImportStmt* parseImport(State& st)
	{
		using TT = lexer::TokenType;
		iceAssert(st.eat() == TT::Import);

		if(st.frontAfterWS() != TT::StringLiteral)
			expectedAfter(st, "string literal", "'import' for module specifier", st.frontAfterWS().str());

		{
			auto ret = new ImportStmt(st.loc(), st.frontAfterWS().str());
			ret->resolvedModule = frontend::resolveImport(ret->path, ret->loc, st.currentFilePath);

			st.eat();

			// check for 'import as foo'
			if(st.frontAfterWS() == TT::As)
			{
				st.eat();
				auto t = st.eat();
				if(t == TT::Underscore || t == TT::Identifier)
					ret->importAs = util::to_string(t.text);

				else
					expectedAfter(st.ploc(), "identifier", "'import-as'", st.prev().str());
			}

			return ret;
		}
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
}

void expected(const Location& loc, std::string a, std::string b)
{
	error(loc, "Expected %s, found '%s' instead", a, b);
}

void expectedAfter(const Location& loc, std::string a, std::string b, std::string c)
{
	error(loc, "Expected %s after %s, found '%s' instead", a, b, c);
}

void unexpected(const Location& loc, std::string a)
{
	error(loc, "Unexpected %s", a);
}













