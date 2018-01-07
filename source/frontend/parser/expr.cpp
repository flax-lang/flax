// expr.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "parser.h"
#include "parser_internal.h"

using namespace ast;
using namespace lexer;



namespace parser
{
	using TT = lexer::TokenType;

	ast::Stmt* parseStmtWithAccessSpec(State& st)
	{
		iceAssert(st.front() == TT::Public || st.front() == TT::Private || st.front() == TT::Internal);
		auto vis = VisibilityLevel::Invalid;
		switch(st.front())
		{
			case TT::Public:		vis = VisibilityLevel::Public; break;
			case TT::Private:		vis = VisibilityLevel::Private; break;
			case TT::Internal:		vis = VisibilityLevel::Internal; break;
			default: iceAssert(0);
		}

		st.pop();
		auto stmt = parseStmt(st);
		if(auto defn = dynamic_cast<FuncDefn*>(stmt))
			defn->visibility = vis;

		else if(auto defn = dynamic_cast<ForeignFuncDefn*>(stmt))
			defn->visibility = vis;

		else if(auto defn = dynamic_cast<VarDefn*>(stmt))
			defn->visibility = vis;

		else if(auto defn = dynamic_cast<TypeDefn*>(stmt))
			defn->visibility = vis;

		else
			error(st, "Access specifier cannot be applied to this statement");

		return stmt;
	}

	ast::Stmt* parseStmt(State& st)
	{
		if(!st.hasTokens())
			unexpected(st, "end of file");

		st.skipWS();

		auto tok = st.front();
		if(tok != TT::EndOfFile)
		{
			switch(tok.type)
			{
				case TT::Var:
				case TT::Val:
					return parseVariable(st);

				case TT::Func:
					return parseFunction(st);

				case TT::ForeignFunc:
					return parseForeignFunction(st);

				case TT::Public:
				case TT::Private:
				case TT::Internal:
					return parseStmtWithAccessSpec(st);

				case TT::If:
					return parseIfStmt(st);

				case TT::Else:
					error(st, "Cannot have 'else' without preceeding 'if'");

				case TT::Return:
					return parseReturn(st);

				case TT::Do:
				case TT::While:
				case TT::Loop:
					return parseWhileLoop(st);

				case TT::For:
					return parseForLoop(st);

				case TT::Break:
					return parseBreak(st);

				case TT::Continue:
					return parseContinue(st);

				case TT::Struct:
					return parseStruct(st);

				case TT::Class:
					return parseClass(st);

				case TT::Enum:
					return parseEnum(st);

				case TT::Static:
					st.pop();
					return new StaticStmt(parseStmt(st));

				case TT::Dealloc:
					return parseDealloc(st);

				case TT::Defer:
					return parseDefer(st);

				case TT::Operator:
					return parseOperatorOverload(st);

				case TT::Protocol:
				case TT::Override:
				case TT::Extension:
				case TT::TypeAlias:
					error(st, "notsup");

				case TT::Namespace:
					error(st, "Namespaces can only be defined at the top-level scope");

				case TT::Import:
					error(st, "All imports must happen at the top-level scope");

				case TT::Attr_Operator:
					error(st, "All operator declarations must happen at the top-level scope");

				case TT::Export:
					error(st, "Export declaration must be the first non-comment line in the file");

				default:
					return parseExpr(st);
			}
		}

		unexpected(st.loc(), "end of file");
		iceAssert(0);
	}


	static bool isRightAssociative(TT tt)
	{
		return false;
	}

	static bool isPostfixUnaryOperator(State& st, const Token& tk)
	{
		bool res = (tk == TT::LParen) || (tk == TT::LSquare) || (tk == TT::DoublePlus) || (tk == TT::DoubleMinus)
			|| (tk == TT::Ellipsis) || (tk == TT::HalfOpenEllipsis);

		if(auto it = st.postfixOps.find(tk.str()); !res && it != st.postfixOps.end())
		{
			iceAssert(it->second.kind == CustomOperatorDecl::Kind::Postfix);
			res = true;
		}

		return res;
	}

	static int unaryPrecedence(const std::string& op)
	{
		if(op == "!" || op == "+" || op == "-" || op == "~" || op == "*" || op == "&")
			return 950;

		return -1;
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
			// () and [] have the same precedence.
			// not sure if this should stay -- works for now.
			case TT::LParen:
			case TT::LSquare:
				return 2000;

			case TT::Period:
				return 1000;

			// unary !
			// unary +/-
			// bitwise ~
			// unary &
			// unary *
			// ^^ all have 950.

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
			case TT::UnicodeSymbol:
				if(auto it = st.binaryOps.find(st.front().str()); it != st.binaryOps.end())
					return it->second.precedence;

				else if(auto it = st.postfixOps.find(st.front().str()); it != st.postfixOps.end())
					return it->second.precedence;

				return -1;

			default:
				return -1;
		}
	}


	static Expr* parseUnary(State& st);
	static Expr* parsePrimary(State& st);
	static Expr* parsePostfixUnary(State& st, Expr* lhs, Token op);

	static Expr* parseRhs(State& st, Expr* lhs, int prio)
	{
		while(true)
		{
			int prec = precedence(st);
			if(prec < prio && !isRightAssociative(st.front()))
				return lhs;

			// we don't really need to check, because if it's botched we'll have returned due to -1 < everything

			if(auto tok_op = st.front(); isPostfixUnaryOperator(st, tok_op))
			{
				st.eat();
				lhs = parsePostfixUnary(st, dynamic_cast<Expr*>(lhs), tok_op);
				continue;
			}

			auto loc = st.loc();
			auto op = parseOperatorTokens(st);
			if(op.empty())
				error(loc, "Invalid operator '%s'", st.prev().str());



			Expr* rhs = 0;
			if(op == "cast")
			{
				rhs = new TypeExpr(loc, parseType(st));
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

			if(op == ".")
			{
				loc.col = lhs->loc.col;
				loc.len = rhs->loc.col - lhs->loc.col + 1;

				lhs = new DotOperator(loc, dynamic_cast<Expr*>(lhs), rhs);
			}
			else if(isAssignOp(op))
			{
				auto newlhs = new AssignOp(loc);

				newlhs->left = dynamic_cast<Expr*>(lhs);
				newlhs->right = rhs;
				newlhs->op = op;

				lhs = newlhs;
			}
			else
			{
				lhs = new BinaryOp(loc, op, dynamic_cast<Expr*>(lhs), rhs);
			}
		}
	}


	static Expr* parseTuple(State& st, Expr* lhs)
	{
		iceAssert(lhs);

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

		Location loc = lhs->loc;
		loc.col -= 1;
		loc.len = (st.front().loc.col - loc.col + 1);
		return new LitTuple(loc, values);
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
		std::string op;

		//* note: we use the binary versions of the operator; since the unary AST and binary AST are separate,
		//* there's no confusion here. It also makes custom operators less troublesome to implement.


		if(tk.type == TT::Exclamation || tk.type == TT::Plus || tk.type == TT::Minus
			|| tk.type == TT::Tilde || tk.type == TT::Asterisk || tk.type == TT::Ampersand)
		{
			op = tk.str();
		}


		int prec = -1;
		if(auto it = st.prefixOps.find(tk.str()); op.empty() && it != st.prefixOps.end())
		{
			// great.
			auto cop = it->second;
			iceAssert(cop.kind == CustomOperatorDecl::Kind::Prefix);

			prec = cop.precedence;
			op = cop.symbol;

			// debuglog("symbol = '%s'\n", op);
		}
		else
		{
			prec = unaryPrecedence(op);
		}



		if(!op.empty())
		{
			st.eat();

			Expr* un = parseUnary(st);
			Expr* thing = parseRhs(st, un, prec);

			auto ret = new UnaryOp(tk.loc);

			ret->expr = thing;
			ret->op = op;

			return ret;
		}
		else
		{
			return parsePrimary(st);
		}
	}


	static std::vector<std::pair<std::string, Expr*>> parseCallArgumentList(State& st)
	{
		std::vector<std::pair<std::string, Expr*>> ret;

		bool named = false;
		while(st.front() != TT::RParen)
		{
			std::string argname;

			auto ex = parseExpr(st);
			if(auto id = dynamic_cast<ast::Ident*>(ex); id && st.front() == TT::Colon)
			{
				argname = id->name;

				// eat the colon, get the actual argument.
				st.eat();
				ex = parseExpr(st);

				named = true;
			}
			else if(named)
			{
				error(st, "Positional arguments cannot appear after named arguments in a function call");
			}

			ret.push_back({ argname, ex });
			st.skipWS();

			if(st.front() == TT::Comma)
				st.pop();

			else if(st.front() != TT::RParen)
				expected(st, "',' or ')' in function call argument list", st.front().str());
		}

		iceAssert(st.pop().type == TT::RParen);

		return ret;
	}

	static FunctionCall* parseFunctionCall(State& st, std::string name)
	{
		auto ret = new FunctionCall(st.ploc(), name);

		st.skipWS();
		ret->args = parseCallArgumentList(st);

		return ret;
	}


	static Expr* parseCall(State& st, Expr* lhs, Token op)
	{
		if(Ident* id = dynamic_cast<Ident*>(lhs))
			return parseFunctionCall(st, id->name);

		auto ret = new ast::ExprCall(op.loc);
		iceAssert(op == TT::LParen);

		ret->args = parseCallArgumentList(st);
		ret->callee = lhs;

		return ret;
	}


	static Expr* parsePostfixUnary(State& st, Expr* lhs, Token op)
	{
		if(op.type == TT::LParen)
		{
			return parseCall(st, lhs, op);
		}
		else if(op.type == TT::LSquare)
		{
			// if we're a colon immediately, then it's a slice expr already.
			// either [:x] or [:]
			Expr* slcbegin = 0;
			Expr* slcend = 0;

			bool isSlice = false;

			if(st.front().type == TT::Colon)
			{
				st.eat();
				isSlice = true;

				if(st.front().type == TT::RSquare)
				{
					st.eat();

					// just return here.
					auto ret = new SliceOp(op.loc);
					ret->expr = lhs;
					ret->start = 0;
					ret->end = 0;

					return ret;
				}
			}

			// parse the inside expression
			Expr* inside = parseExpr(st);

			if(!isSlice && st.front().type == TT::RSquare)
			{
				auto end = st.eat().loc;
				iceAssert(inside);

				auto ret = new SubscriptOp(op.loc);
				ret->expr = lhs;
				ret->inside = inside;

				// custom thingy
				ret->loc.col = lhs->loc.col;
				ret->loc.len = end.col - ret->loc.col + 1;

				return ret;
			}
			else if(isSlice && st.front().type == TT::RSquare)
			{
				st.eat();

				// x[:N]
				slcbegin = 0;
				slcend = inside;

				auto ret = new SliceOp(op.loc);
				ret->expr = lhs;
				ret->start = slcbegin;
				ret->end = slcend;

				return ret;
			}
			else if(st.front().type == TT::Colon)
			{
				st.eat();
				// x[N:M]
				slcbegin = inside;
				slcend = parseExpr(st);

				if(st.front() != TT::RSquare)
					expectedAfter(st.loc(), "']'", "expression for slice operation", st.front().str());

				st.eat();

				auto ret = new SliceOp(op.loc);
				ret->expr = lhs;
				ret->start = slcbegin;
				ret->end = slcend;

				return ret;
			}
			else
			{
				expectedAfter(st.loc(), "']'", "'[' for array subscript", st.front().str());
			}
		}
		else if(op.type == TT::Ellipsis || op.type == TT::HalfOpenEllipsis)
		{
			Expr* start = lhs;
			iceAssert(start);

			// current token now starts the ending expression (ie. '...' or '..<' are no longer in the token stream)
			Expr* end = parseExpr(st);
			Expr* step = 0;

			// check if we got a step.
			if(st.front() == TT::Identifier && st.front().text == "step")
			{
				st.eat();
				step = parseExpr(st);
			}

			// ok
			auto ret = new RangeExpr(op.loc);
			ret->start = start;
			ret->end = end;
			ret->halfOpen = (op.type == TT::HalfOpenEllipsis);
			ret->step = step;

			return ret;
		}
		else
		{
			// todo: ++ and --.
			error("enotsup");
		}
	}


	static Expr* parseAlloc(State& st, bool raw)
	{
		iceAssert(st.front() == TT::Alloc);
		auto loc = st.eat().loc;

		if(st.eat() != TT::LParen)
			expectedAfter(st.loc(), "'('", "'alloc'", st.front().str());

		// ok, there are 2 forms here:
		// 1. alloc(T)
		// 2. alloc(T, N, ...)

		auto ret = new ast::AllocOp(loc);
		ret->isRaw = raw;
		ret->allocTy = parseType(st);

		if(st.front() == TT::Comma)
		{
			st.eat();

			while(st.front() != TT::RParen)
			{
				ret->counts.push_back(parseExpr(st));
				if(st.front() == TT::Comma)
					st.eat();

				else if(st.front() != TT::RParen)
					expected(st.loc(), "',' or ')' in dimension list for alloc expression", st.front().str());
			}

			iceAssert(st.front() == TT::RParen);
		}

		if(st.eat() != TT::RParen)
			expectedAfter(st.ploc(), "')'", "'alloc'", st.prev().str());

		return ret;
	}

	ast::DeallocOp* parseDealloc(State& st)
	{
		iceAssert(st.front() == TT::Dealloc);
		auto loc = st.eat().loc;

		auto ret = new ast::DeallocOp(loc);
		ret->expr = parseExpr(st);

		return ret;
	}

	ast::DeferredStmt* parseDefer(State& st)
	{
		iceAssert(st.front() == TT::Defer);
		auto ret = new ast::DeferredStmt(st.eat().loc);

		if(st.front() == TT::LBrace)
			ret->actual = parseBracedBlock(st);

		else
			ret->actual = parseStmt(st);

		return ret;
	}




	Expr* parseExpr(State& st)
	{
		Expr* lhs = parseUnary(st);
		iceAssert(lhs);

		return parseRhs(st, lhs, 0);
	}



	static Expr* parseIdentifier(State& st)
	{
		iceAssert(st.front() == TT::Identifier || st.front() == TT::UnicodeSymbol);
		std::string name = st.front().str();
		st.pop();

		return new Ident(st.ploc(), name);
	}




	static Expr* parsePrimary(State& st)
	{
		if(!st.hasTokens())
			unexpected(st, "end of file");

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
					error(st, "Declarations are not expressions (%s)", tok.str());

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
					error(st, "If statements are not expressions");

				case TT::Do:
				case TT::While:
				case TT::Loop:
				case TT::For:
					error(st, "Loops are not expressions");







				case TT::LParen:
					return parseParenthesised(st);

				case TT::Identifier:
				case TT::UnicodeSymbol:
					return parseIdentifier(st);

				case TT::Alloc:
					return parseAlloc(st, false);

				// case TT::Typeof:
				// 	return parseTypeof(ps);

				// case TT::Typeid:
				// 	return parseTypeid(ps);

				// case TT::Sizeof:
				// 	return parseSizeof(ps);


				case TT::Attr_Raw:
					st.pop();
					if(st.front() == TT::StringLiteral)
						return parseString(st, true);

					else if(st.front() == TT::LSquare)
						return parseArray(st, true);

					else if(st.front() == TT::Alloc)
						return parseAlloc(st, true);

					else
						expectedAfter(st, "string literal or ", "@raw", st.front().str());

				case TT::StringLiteral:
					return parseString(st, false);

				case TT::Number:
					return parseNumber(st);

				case TT::LSquare:
					return parseArray(st, false);

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
					unexpected(st, "block; to create a nested scope, use 'do { ... }'");

				default:
					error(tok.loc, "Unexpected token '%s' (id = %d)", tok.str(), tok.type);
			}
		}

		return 0;
	}













}
























