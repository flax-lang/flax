// expr.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "parser_internal.h"

using namespace ast;
using namespace lexer;



namespace parser
{
	using TT = TokenType;

	static bool isRightAssociative(TT tt)
	{
		return false;
	}

	static bool isPostfixUnaryOperator(TT tt)
	{
		return (tt == TT::LSquare) || (tt == TT::DoublePlus) || (tt == TT::DoubleMinus)
			|| (tt == TT::Ellipsis) || (tt == TT::HalfOpenEllipsis);
	}

	static int unaryPrecedence(Operator op)
	{
		switch(op)
		{
			case Operator::LogicalNot:
			case Operator::Plus:
			case Operator::Minus:
			case Operator::BitwiseNot:
			case Operator::Dereference:
			case Operator::AddressOf:
				return 950;

			default:
				return -1;
		}
	}

	static int precedence(State& st)
	{
		if(st.remaining() > 1 && (st.front() == TT::LAngle || st.front() == TT::RAngle))
		{
			// check if the next one matches.
			if(st.front().type == TT::LAngle && st.lookahead(1).type == TT::LAngle)
				return 650;

			else if(st.front().type == TT::RAngle && st.lookahead(1).type == TT::RAngle)
				return 650;


			else if(st.front().type == TT::LAngle && st.lookahead(1).type == TT::LessThanEquals)
				return 100;

			else if(st.front().type == TT::RAngle && st.lookahead(1).type == TT::GreaterEquals)
				return 100;
		}

		switch(st.front())
		{
			// . and [] have the same precedence.
			// not sure if this should stay -- works for now.
			case TT::Period:
			case TT::LSquare:
				return 1000;

			// unary !
			// unary +/-
			// bitwise ~
			// unary &
			// unary *
			case TT::As:
				return 900;

			case TT::DoublePlus:
			case TT::DoubleMinus:
				return 850;

			case TT::Asterisk:
			case TT::Divide:
			case TT::Percent:
				return 800;

			case TT::Plus:
			case TT::Minus:
				return 750;

			// << and >>
			// precedence = 700


			case TT::Ampersand:
				return 650;

			case TT::Caret:
				return 600;

			case TT::Pipe:
				return 550;

			case TT::LAngle:
			case TT::RAngle:
			case TT::LessThanEquals:
			case TT::GreaterEquals:
				return 500;

			case TT::Ellipsis:
			case TT::HalfOpenEllipsis:
				return 475;

			case TT::EqualsTo:
			case TT::NotEquals:
				return 450;

			case TT::LogicalAnd:
				return 400;

			case TT::LogicalOr:
				return 350;

			case TT::Equal:
			case TT::PlusEq:
			case TT::MinusEq:
			case TT::MultiplyEq:
			case TT::DivideEq:
			case TT::ModEq:
			case TT::AmpersandEq:
			case TT::PipeEq:
			case TT::CaretEq:
				return 100;


			case TT::ShiftLeftEq:
			case TT::ShiftRightEq:
			case TT::ShiftLeft:
			case TT::ShiftRight:
				iceAssert(0);	// note: handled above, should not reach here
				break;

			case TT::Identifier:
			// case TT::UnicodeSymbol:
			// 	if(st.cgi->customOperatorMapRev.find(st.front().text.to_string()) != st.cgi->customOperatorMapRev.end())
			// 	{
			// 		return st.cgi->customOperatorMap[st.cgi->customOperatorMapRev[st.front().text.to_string()]].second;
			// 	}
				return -1;

			default:
				return -1;
		}
	}


	static Expr* parseUnary(State& st);
	static Expr* parsePrimary(State& st);

	static Expr* parseRhs(State& st, Expr* lhs, int prio)
	{
		while(true)
		{
			int prec = precedence(st);
			if(prec < prio && !isRightAssociative(st.front()))
				return lhs;

			// we don't really need to check, because if it's botched we'll have returned due to -1 < everything

			Token tok_op = st.eat();
			Token next1 = st.front();

			auto op = Operator::Invalid;
			if(tok_op == TT::LAngle || tok_op == TT::RAngle)
			{
				// check if the next one matches.
				if(tok_op == TT::LAngle)
				{
					if(next1 == TT::LAngle)
					{
						// < < is <<
						op = Operator::ShiftLeft;
						st.eat();
					}
					else if(next1 == TT::LessThanEquals)
					{
						// < <= is <<=
						op = Operator::ShiftLeftEquals;
						st.eat();
					}
				}
				else if(tok_op == TT::RAngle)
				{
					if(next1 == TT::RAngle)
					{
						// > > is >>
						op = Operator::ShiftRight;
						st.eat();
					}
					else if(next1 == TT::GreaterEquals)
					{
						// > >= is >>=
						op = Operator::ShiftRightEquals;
						st.eat();
					}
				}
			}
			else if(isPostfixUnaryOperator(tok_op))
			{
				// lhs = parsePostfixUnaryOp(st, tok_op, lhs);
				error("notsup");
				continue;
			}



			if(op == Operator::Invalid)
			{
				switch(tok_op.type)
				{
					case TT::Plus:				op = Operator::Add;					break;
					case TT::Minus:				op = Operator::Subtract;			break;
					case TT::Asterisk:			op = Operator::Multiply;			break;
					case TT::Divide:			op = Operator::Divide;				break;
					case TT::Percent:			op = Operator::Modulo;				break;
					case TT::ShiftLeft:			op = Operator::ShiftLeft;			break;
					case TT::ShiftRight:		op = Operator::ShiftRight;			break;
					case TT::Equal:				op = Operator::Assign;				break;

					case TT::LAngle:			op = Operator::CompareLess;			break;
					case TT::RAngle:			op = Operator::CompareGreater;		break;
					case TT::LessThanEquals:	op = Operator::CompareLessEq;		break;
					case TT::GreaterEquals:		op = Operator::CompareGreaterEq;	break;
					case TT::EqualsTo:			op = Operator::CompareEq;			break;
					case TT::NotEquals:			op = Operator::CompareNotEq;		break;

					case TT::Ampersand:			op = Operator::BitwiseAnd;			break;
					case TT::Pipe:				op = Operator::BitwiseOr;			break;
					case TT::Caret:				op = Operator::BitwiseXor;			break;
					case TT::LogicalOr:			op = Operator::LogicalOr;			break;
					case TT::LogicalAnd:		op = Operator::LogicalAnd;			break;

					case TT::PlusEq:			op = Operator::PlusEquals;			break;
					case TT::MinusEq:			op = Operator::MinusEquals;			break;
					case TT::MultiplyEq:		op = Operator::MultiplyEquals;		break;
					case TT::DivideEq:			op = Operator::DivideEquals;		break;
					case TT::ModEq:				op = Operator::ModuloEquals;		break;
					case TT::ShiftLeftEq:		op = Operator::ShiftLeftEquals;		break;
					case TT::ShiftRightEq:		op = Operator::ShiftRightEquals;	break;
					case TT::AmpersandEq:		op = Operator::BitwiseAndEquals;	break;
					case TT::PipeEq:			op = Operator::BitwiseOrEquals;		break;
					case TT::CaretEq:			op = Operator::BitwiseXorEquals;	break;

					case TT::Period:			op = Operator::DotOperator;			break;
					case TT::As:				op = Operator::Cast;				break;

					default:
						error(st, "Unknown operator '%s'", tok_op.str().c_str());
				}
			}



			Expr* rhs = 0;
			if(tok_op.type == TT::As)
			{
				rhs = new TypeExpr(tok_op.loc, parseType(st));
			}
			else
			{
				rhs = parseUnary(st);
			}

			int next = precedence(st);

			if(next > prec || isRightAssociative(st.front()))
				rhs = parseRhs(st, rhs, prec + 1);

			// todo: chained relational operators
			// eg. 1 == 1 < 4 > 3 > -5 == -7 + 2 < 10 > 3

			if(op == Operator::DotOperator)
				lhs = new DotOperator(tok_op.loc, lhs, rhs);

			else
				lhs = new BinaryOp(tok_op.loc, op, lhs, rhs);
		}
	}


















	static Expr* parseTuple(State& st, Expr* lhs)
	{
		iceAssert(lhs);

		Token first = st.front();
		std::vector<Expr*> values { lhs };

		Token t = st.front();
		while(true)
		{
			values.push_back(parseExpr(st));
			if(st.front().type == TT::RParen)
				break;

			if(st.front().type != TT::Comma)
				expected(st, "either ')' or ',' in tuple", st.front().str());

			st.eat();
			t = st.front();
		}

		// leave the last rparen
		iceAssert(st.front().type == TT::RParen);

		// return CreateAST(Tuple, first, values);
		return new LitTuple(first.loc, values);
	}


	static Expr* parseParenthesised(State& st)
	{
		Token opening = st.eat();
		iceAssert(opening.type == TT::LParen);

		Expr* within = parseExpr(st);

		if(st.front().type != TT::Comma && st.front().type != TT::RParen)
			error(opening.loc, "Expected closing ')' to match opening parenthesis here, or ',' to begin a tuple");

		// if we're a tuple, get ready for this shit.
		if(st.front().type == TT::Comma)
		{
			// remove the comma
			st.eat();

			// parse a tuple
			Expr* tup = parseTuple(st, within);
			within = tup;
		}

		iceAssert(st.front().type == TT::RParen);
		st.eat();

		return within;
	}

	static Expr* parseUnary(State& st)
	{
		auto tk = st.front();

		// check for unary shit
		auto op = Operator::Invalid;

		if(tk.type == TT::Exclamation)		op = Operator::LogicalNot;
		else if(tk.type == TT::Plus)		op = Operator::Plus;
		else if(tk.type == TT::Minus)		op = Operator::Minus;
		else if(tk.type == TT::Tilde)		op = Operator::BitwiseNot;
		else if(tk.type == TT::Asterisk)	op = Operator::Dereference;
		else if(tk.type == TT::Ampersand)	op = Operator::AddressOf;

		if(op != Operator::Invalid)
		{
			st.eat();

			int prec = unaryPrecedence(op);
			Expr* un = parseUnary(st);
			Expr* thing = parseRhs(st, un, prec);

			auto ret = new UnaryOp(tk.loc);

			ret->expr = thing;
			ret->op = op;

			return ret;
		}

		return parsePrimary(st);
	}

	Expr* parseExpr(State& st)
	{
		Expr* lhs = parseUnary(st);
		iceAssert(lhs);

		return parseRhs(st, lhs, 0);
	}












	static Expr* parsePrimary(State& st)
	{
		if(!st.hasTokens())
			error(st, "Unexpected end of file");

		st.skipWS();

		auto tok = st.front();
		if(tok != TT::EndOfFile)
		{
			switch(tok.type)
			{
				case TT::Var:
				case TT::Val:
				case TT::Func:
				case TT::Enum:
				case TT::Class:
				case TT::Static:
				case TT::Struct:
				case TT::Public:
				case TT::Private:
				case TT::Operator:
				case TT::Protocol:
				case TT::Internal:
				case TT::Override:
				case TT::Extension:
				case TT::Namespace:
				case TT::TypeAlias:
				case TT::ForeignFunc:
					error(st, "Declarations are not expressions (%s)", tok.str().c_str());

				case TT::Dealloc:
					error(st, "Deallocation statements are not expressions");

				case TT::Defer:
					error(st, "Defer statements are not expressions");

				case TT::Return:
					error(st, "Return statements are not expressions");

				case TT::Break:
					error(st, "Break statements are not expressions");

				case TT::Continue:
					error(st, "Continue statements are not expressions");

				case TT::If:
					error(st, "If statements are not expressions (yet)");

				case TT::Do:
				case TT::While:
				case TT::Loop:
				case TT::For:
					error(st, "Loops are not expressions");







				case TT::LParen:
					return parseParenthesised(st);

				// case TT::Identifier:
				// case TT::UnicodeSymbol:
				// 	if(tok.text == "init")
				// 		return parseInitFunc(ps);

				// 	return parseIdExpr(ps);

				// case TT::Alloc:
				// 	return parseAlloc(ps);

				// case TT::Typeof:
				// 	return parseTypeof(ps);

				// case TT::Typeid:
				// 	return parseTypeid(ps);

				// case TT::Sizeof:
				// 	return parseSizeof(ps);

				// case TT::StringLiteral:
				// 	return parseStringLiteral(ps);

				case TT::Number:
					return parseNumber(st);

				// case TT::LSquare:
				// 	return parseArrayLiteral(ps);

				// case TT::At:
				// 	parseAttribute(ps);
				// 	return parsePrimary(ps);

				// no point creating separate functions for these
				case TT::True:
					st.pop();
					return new LitBool(tok.loc, true);

				case TT::False:
					st.pop();
					return new LitBool(tok.loc, false);

				// nor for this
				case TT::Null:
					st.pop();
					return new LitNull(tok.loc);


				case TT::LBrace:
					// parse it, but throw it away
					// warn(parseBracedBlock(st)->loc, "Anonymous blocks are ignored; to run, preface with 'do'");
					// return 0;
					error(st, "Unexpected block; to create a nested scope, use 'do { ... }'");

				default:
					error(tok.loc, "Unexpected token '%s' (id = %d)", tok.str().c_str(), tok.type);
			}
		}

		return 0;
	}

}
























