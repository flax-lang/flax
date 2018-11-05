// operators.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "defs.h"
#include "pts.h"
#include "parser.h"
#include "parser_internal.h"

#include "mpool.h"


using namespace ast;
using namespace lexer;

using TT = lexer::TokenType;
namespace parser
{
	OperatorOverloadDefn* parseOperatorOverload(State& st)
	{
		using Kind = OperatorOverloadDefn::Kind;

		iceAssert(st.front() == TT::Operator);
		auto ret = util::pool<OperatorOverloadDefn>(st.eat().loc);

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




		bool isvar = false;
		std::tie(ret->args, ret->generics, ret->returnType, isvar, std::ignore) = parseFunctionLookingDecl(st);

		if(ret->returnType == 0)
			ret->returnType = pts::NamedType::create(VOID_TYPE_STRING);

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
			case TT::Plus:              return Operator::Plus;
			case TT::Minus:             return Operator::Minus;
			case TT::Asterisk:          return Operator::Multiply;
			case TT::Divide:            return Operator::Divide;
			case TT::Percent:           return Operator::Modulo;
			case TT::ShiftLeft:         return Operator::BitwiseShiftLeft;
			case TT::ShiftRight:        return Operator::BitwiseShiftRight;
			case TT::Equal:             return Operator::Assign;

			case TT::LAngle:            return Operator::CompareLT;
			case TT::RAngle:            return Operator::CompareGT;
			case TT::LessThanEquals:    return Operator::CompareLEQ;
			case TT::GreaterEquals:     return Operator::CompareGEQ;
			case TT::EqualsTo:          return Operator::CompareEQ;
			case TT::NotEquals:         return Operator::CompareNEQ;

			case TT::Ampersand:         return Operator::BitwiseAnd;
			case TT::Pipe:              return Operator::BitwiseOr;
			case TT::Caret:             return Operator::BitwiseXor;
			case TT::LogicalOr:         return Operator::LogicalOr;
			case TT::LogicalAnd:        return Operator::LogicalAnd;

			case TT::PlusEq:            return Operator::PlusEquals;
			case TT::MinusEq:           return Operator::MinusEquals;
			case TT::MultiplyEq:        return Operator::MultiplyEquals;
			case TT::DivideEq:          return Operator::DivideEquals;
			case TT::ModEq:             return Operator::ModuloEquals;
			case TT::ShiftLeftEq:       return Operator::BitwiseShiftLeftEquals;
			case TT::ShiftRightEq:      return Operator::BitwiseShiftRightEquals;
			case TT::AmpersandEq:       return Operator::BitwiseAndEquals;
			case TT::PipeEq:            return Operator::BitwiseOrEquals;
			case TT::CaretEq:           return Operator::BitwiseXorEquals;

			case TT::As:                return Operator::TypeCast;
			case TT::Is:                return Operator::TypeIs;

			case TT::Period:            return ".";
			case TT::At:                return "@";

			case TT::Ellipsis:          return "...";
			case TT::HalfOpenEllipsis:  return "..<";

			default:                    break;
		}

		// check custom operators.
		if(tok_op.type == TT::Identifier || tok_op.type == TT::UnicodeSymbol)
			return tok_op.str();

		return "";
	}

	size_t parseOperatorDecl(const lexer::TokenList& tokens, size_t idx, int* _kind, CustomOperatorDecl* out)
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
		if(tokens[idx].str() == "infix")        kind = 1, oper.kind = Kind::Infix;
		else if(tokens[idx].str() == "prefix")  kind = 2, oper.kind = Kind::Prefix;
		else if(tokens[idx].str() == "postfix") kind = 3, oper.kind = Kind::Postfix;
		else                                    iceAssert(0);

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
			error(tokens[idx].loc, "expected newline or semicolon to terminate operator declaration, found '%s'", tokens[idx].str());
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










namespace Operator
{
	const std::string Plus                      = "+";
	const std::string Minus                     = "-";
	const std::string Multiply                  = "*";
	const std::string Divide                    = "/";
	const std::string Modulo                    = "%";

	const std::string UnaryPlus                 = "+";
	const std::string UnaryMinus                = "-";

	const std::string PointerDeref              = "*";
	const std::string AddressOf                 = "&";

	const std::string BitwiseNot                = "~";
	const std::string BitwiseAnd                = "&";
	const std::string BitwiseOr                 = "|";
	const std::string BitwiseXor                = "^";
	const std::string BitwiseShiftLeft          = "<<";
	const std::string BitwiseShiftRight         = ">>";

	const std::string LogicalNot                = "!";
	const std::string LogicalAnd                = "&&";
	const std::string LogicalOr                 = "||";

	const std::string CompareEQ                 = "==";
	const std::string CompareNEQ                = "!=";
	const std::string CompareLT                 = "<";
	const std::string CompareLEQ                = "<=";
	const std::string CompareGT                 = ">";
	const std::string CompareGEQ                = ">=";

	const std::string Assign                    = "=";
	const std::string PlusEquals                = "+=";
	const std::string MinusEquals               = "-=";
	const std::string MultiplyEquals            = "*=";
	const std::string DivideEquals              = "/=";
	const std::string ModuloEquals              = "%=";
	const std::string BitwiseShiftLeftEquals    = "<<=";
	const std::string BitwiseShiftRightEquals   = ">>=";
	const std::string BitwiseXorEquals          = "^=";
	const std::string BitwiseAndEquals          = "&=";
	const std::string BitwiseOrEquals           = "|=";

	const std::string TypeCast                  = "as";
	const std::string TypeIs                    = "is";


	bool isArithmetic(const std::string& op)
	{
		return (op == Operator::Plus || op == Operator::Minus || op == Operator::Multiply || op == Operator::Divide || op == Operator::Modulo);
	}

	bool isBitwise(const std::string& op)
	{
		return (op == Operator::BitwiseAnd || op == Operator::BitwiseOr || op == Operator::BitwiseXor
			|| op == Operator::BitwiseShiftLeft || op == Operator::BitwiseShiftRight);
	}

	bool isAssignment(const std::string& op)
	{
		return (op == Operator::Assign || op == Operator::PlusEquals || op == Operator::MinusEquals || op == Operator::MultiplyEquals
			|| op == Operator::DivideEquals || op == Operator::ModuloEquals || op == Operator::BitwiseShiftLeftEquals
			|| op == Operator::BitwiseShiftRightEquals || op == Operator::BitwiseAndEquals || op == Operator::BitwiseOrEquals
			|| op == Operator::BitwiseXorEquals);
	}

	bool isComparison(const std::string& op)
	{
		return (op == Operator::CompareEQ || op == Operator::CompareNEQ || op == Operator::CompareLT || op == Operator::CompareGT
			|| op == Operator::CompareLEQ || op == Operator::CompareGEQ);
	}


	std::string getNonAssignmentVersion(const std::string& op)
	{
		if(op == Operator::PlusEquals)                      return Operator::Plus;
		else if(op == Operator::MinusEquals)                return Operator::Minus;
		else if(op == Operator::MultiplyEquals)             return Operator::Multiply;
		else if(op == Operator::DivideEquals)               return Operator::Divide;
		else if(op == Operator::ModuloEquals)               return Operator::Modulo;
		else if(op == Operator::BitwiseShiftLeftEquals)     return Operator::BitwiseShiftLeft;
		else if(op == Operator::BitwiseShiftRightEquals)    return Operator::BitwiseShiftRight;
		else if(op == Operator::BitwiseAndEquals)           return Operator::BitwiseAnd;
		else if(op == Operator::BitwiseOrEquals)            return Operator::BitwiseOr;
		else if(op == Operator::BitwiseXorEquals)           return Operator::BitwiseXor;

		error("no");
	}



}
















