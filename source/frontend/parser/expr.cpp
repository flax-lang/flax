// expr.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "parser.h"
#include "parser_internal.h"

#include "mpool.h"

using namespace ast;
using namespace lexer;



namespace parser
{
	using TT = lexer::TokenType;

	Stmt* parseStmtWithAccessSpec(State& st)
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
		if(auto defn = dcast(FuncDefn, stmt))
			defn->visibility = vis;

		else if(auto defn = dcast(ForeignFuncDefn, stmt))
			defn->visibility = vis;

		else if(auto defn = dcast(VarDefn, stmt))
			defn->visibility = vis;

		else if(auto defn = dcast(TypeDefn, stmt))
			defn->visibility = vis;

		else
			error(st, "access specifier cannot be applied to this statement");

		return stmt;
	}

	Stmt* parseStmt(State& st)
	{
		if(!st.hasTokens())
			unexpected(st, "end of file");

		st.skipWS();

		std::function<Stmt* (bool, bool, bool)> checkMethodModifiers = [&st, &checkMethodModifiers](bool mut, bool virt, bool ovrd) -> Stmt* {
			if(st.front() == TT::Mutable)
			{
				if(mut) error(st.loc(), "duplicate 'mut' modifier");

				st.eat();
				return checkMethodModifiers(true, virt, ovrd);
			}
			else if(st.front() == TT::Virtual)
			{
				if(virt) error(st.loc(), "duplicate 'virtual' modifier");
				if(ovrd) error(st.loc(), "'override' implies 'virtual', just use 'override'");

				st.eat();

				//* but virtual does not imply override
				return checkMethodModifiers(mut, true, ovrd);
			}
			else if(st.front() == TT::Override)
			{
				if(ovrd) error(st.loc(), "duplicate 'override' modifier");
				if(virt) error(st.loc(), "'override' implies 'virtual', just use 'override'");

				st.eat();

				//* override implies virtual
				return checkMethodModifiers(mut, true, true);
			}
			else
			{
				auto ret = parseFunction(st);
				ret->isVirtual = virt;
				ret->isOverride = ovrd;
				ret->isMutating = mut;

				return ret;
			}
		};

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
					error(st, "cannot have 'else' without preceeding 'if'");

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

				case TT::Union:
					return parseUnion(st);

				case TT::Struct:
					return parseStruct(st);

				case TT::Class:
					return parseClass(st);

				case TT::Enum:
					return parseEnum(st);

				case TT::Static:
					return parseStaticDecl(st);

				case TT::Dealloc:
					return parseDealloc(st);

				case TT::Defer:
					return parseDefer(st);

				case TT::Operator:
					return parseOperatorOverload(st);

				case TT::Using:
					return parseUsingStmt(st);

				case TT::Mutable:
				case TT::Virtual:
				case TT::Override:
					return checkMethodModifiers(false, false, false);

				case TT::Protocol:
				case TT::Extension:
				case TT::TypeAlias:
					error(st, "notsup");

				case TT::Namespace:
					error(st, "namespaces can only be defined at the top-level scope");

				case TT::Import:
					error(st, "all imports must happen at the top-level scope");

				case TT::Attr_Operator:
					error(st, "all operator declarations must happen at the top-level scope");

				case TT::Export:
					error(st, "export declaration must be the first non-comment line in the file");

				default:
					if(st.isInStructBody() && tok.type == TT::Identifier && tok.str() == "init")
						return parseInitFunction(st);

					return parseExpr(st);
			}
		}

		unexpected(st.loc(), "end of file");
	}


	static bool isRightAssociative(TT tt)
	{
		return false;
	}

	static bool isPostfixUnaryOperator(State& st, const Token& tk)
	{
		bool res = (tk == TT::LParen) || (tk == TT::LSquare) || (tk == TT::DoublePlus) || (tk == TT::DoubleMinus);

		if(auto it = st.postfixOps.find(tk.str()); !res && it != st.postfixOps.end())
		{
			iceAssert(it->second.kind == CustomOperatorDecl::Kind::Postfix);
			res = true;
		}

		return res;
	}

	static int unaryPrecedence(const std::string& op)
	{
		if(op == "!" || op == "+" || op == "-" || op == "~" || op == "*" || op == "&" || op == "...")
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
			case TT::DoubleColon:
				return 1000;

			// unary !
			// unary +/-
			// bitwise ~
			// unary &
			// unary *
			// unary ... (splat)
			// ^^ all have 950.

			case TT::As:
			case TT::Is:
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
			case TT::EqualsTo:
			case TT::NotEquals:
				return 500;

			case TT::Ellipsis:
			case TT::HalfOpenEllipsis:
				return 475;

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
				error("no");
				break;

			default:
				if(auto it = st.binaryOps.find(st.front().str()); it != st.binaryOps.end())
					return it->second.precedence;

				else if(auto it = st.postfixOps.find(st.front().str()); it != st.postfixOps.end())
					return it->second.precedence;

				return -1;
		}
	}


	static Expr* parseUnary(State& st);
	static Expr* parsePrimary(State& st);
	static Expr* parsePostfixUnary(State& st, Expr* lhs, Token op);

	static Expr* parseRhs(State& st, Expr* lhs, int prio)
	{
		if(!st.hasTokens() || st.front() == TT::EndOfFile)
			return lhs;

		while(true)
		{
			int prec = precedence(st);
			if((prec < prio && !isRightAssociative(st.front())) || (st.front() == TT::As && st.isParsingUsing()))
				return lhs;

			// we don't really need to check, because if it's botched we'll have returned due to -1 < everything

			if(auto tok_op = st.front(); isPostfixUnaryOperator(st, tok_op))
			{
				st.eat();
				lhs = parsePostfixUnary(st, dcast(Expr, lhs), tok_op);
				continue;
			}

			auto loc = st.loc();
			auto op = parseOperatorTokens(st);
			if(op.empty())
				error(loc, "invalid operator '%s'", st.prev().str());



			Expr* rhs = 0;
			if(op == Operator::TypeCast)
			{
				if(st.front() == TT::Mutable || (st.front() == TT::Exclamation && st.lookahead(1) == TT::Mutable))
				{
					if(st.front() == TT::Mutable)
					{
						st.pop();
						rhs = util::pool<MutabilityTypeExpr>(loc, true);
					}
					else
					{
						st.pop();
						st.pop();
						rhs = util::pool<MutabilityTypeExpr>(loc, false);
					}
				}
				else
				{
					// rhs = util::pool<TypeExpr>(loc, parseType(st));
					rhs = parseExpr(st);
				}
			}
			else if(op == Operator::TypeIs)
			{
				// rhs = util::pool<TypeExpr>(loc, parseType(st));
				rhs = parseExpr(st);
			}
			else
			{
				rhs = parseUnary(st);
			}

			int next = precedence(st);

			if(next > prec || isRightAssociative(st.front()))
				rhs = parseRhs(st, rhs, prec + 1);


			if(op == "." || op == "::")
			{
				loc.col = lhs->loc.col;
				loc.len = rhs->loc.col + rhs->loc.len - lhs->loc.col;

				auto dot = util::pool<DotOperator>(loc, dcast(Expr, lhs), rhs);
				dot->isStatic = (op == "::");

				lhs = dot;
			}
			else if(op == "..." || op == "..<")
			{
				Expr* start = lhs;
				iceAssert(start);

				// current token now starts the ending expression (ie. '...' or '..<' are no longer in the token stream)
				Expr* end = rhs;
				Expr* step = 0;

				// check if we got a step.
				if(st.front() == TT::Identifier && st.front().text == "step")
				{
					st.eat();
					step = parseExpr(st);
				}

				// ok
				auto ret = util::pool<RangeExpr>(loc);
				ret->start = start;
				ret->end = end;
				ret->halfOpen = (op == "..<");
				ret->step = step;

				lhs = ret;
			}
			else if(Operator::isAssignment(op))
			{
				auto newlhs = util::pool<AssignOp>(loc);

				newlhs->left = dcast(Expr, lhs);
				newlhs->right = rhs;
				newlhs->op = op;

				lhs = newlhs;
			}
			else if(Operator::isComparison(op))
			{
				if(auto cmp = dcast(ComparisonOp, lhs))
				{
					cmp->ops.push_back({ op, loc });
					cmp->exprs.push_back(rhs);
					lhs = cmp;
				}
				else
				{
					iceAssert(lhs);
					auto newlhs = util::pool<ComparisonOp>(loc);
					newlhs->exprs.push_back(lhs);
					newlhs->exprs.push_back(rhs);
					newlhs->ops.push_back({ op, loc });

					lhs = newlhs;
				}
			}
			else
			{
				lhs = util::pool<BinaryOp>(loc, op, dcast(Expr, lhs), rhs);
			}
		}
	}


	Expr* parseCaretScopeExpr(State& st)
	{
		return 0;
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
		return util::pool<LitTuple>(loc, values);
	}

	Expr* parseDollarExpr(State& st)
	{
		if(st.isInSubscript())
		{
			return util::pool<SubscriptDollarOp>(st.pop().loc);
		}
		else
		{
			unexpected(st.loc(), "'$' in non-subscript context");
		}
	}


	static Expr* parseParenthesised(State& st)
	{
		Token opening = st.eat();
		iceAssert(opening.type == TT::LParen);

		if(st.front() == TT::RParen)
			error(st.loc(), "empty tuples are not supported");

		Expr* within = parseExpr(st);
		st.skipWS();

		if(st.front().type != TT::Comma && st.front().type != TT::RParen)
			expected(opening.loc, "closing ')' to match opening parenthesis here, or ',' to begin a tuple", st.front().str());

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
			|| tk.type == TT::Tilde || tk.type == TT::Asterisk || tk.type == TT::Ampersand
			|| tk.type == TT::Ellipsis)
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


		if(op == "...")
		{
			st.eat();

			Expr* e = parseUnary(st);

			auto ret = util::pool<SplatOp>(tk.loc);
			ret->expr = e;

			return ret;
		}
		else if(!op.empty())
		{
			st.eat();

			Expr* un = parseUnary(st);
			Expr* thing = parseRhs(st, un, prec);

			auto ret = util::pool<UnaryOp>(tk.loc);

			ret->expr = thing;
			ret->op = op;

			return ret;
		}
		else
		{
			return parsePrimary(st);
		}
	}


	std::vector<std::pair<std::string, Expr*>> parseCallArgumentList(State& st)
	{
		std::unordered_set<std::string> seenNames;
		std::vector<std::pair<std::string, Expr*>> ret;

		bool named = false;
		while(st.front() != TT::RParen)
		{
			std::string argname;

			auto ex = parseExpr(st);
			if(auto id = dcast(Ident, ex); id && st.front() == TT::Colon)
			{
				argname = id->name;
				if(seenNames.find(argname) != seenNames.end())
					error(id->loc, "argument '%s' was already provided", argname);

				seenNames.insert(argname);

				// eat the colon, get the actual argument.
				st.eat();
				ex = parseExpr(st);

				named = true;
			}
			else if(named)
			{
				error(st, "positional arguments cannot appear after named arguments in a function call");
			}

			ret.push_back({ argname, ex });
			st.skipWS();

			if(st.front() == TT::Comma)
				st.pop();

			else if(st.front() != TT::RParen)
				expected(st, "',' or ')' in function call argument list", st.front().str());
		}

		iceAssert(st.front().type == TT::RParen);
		st.pop();

		return ret;
	}

	static FunctionCall* parseFunctionCall(State& st, std::string name)
	{
		auto ret = util::pool<FunctionCall>(st.lookahead(-2).loc, name);

		st.skipWS();
		ret->args = parseCallArgumentList(st);

		return ret;
	}


	static Expr* parseCall(State& st, Expr* lhs, Token op)
	{
		if(Ident* id = dcast(Ident, lhs))
		{
			auto ret = parseFunctionCall(st, id->name);
			ret->mappings = id->mappings;

			return ret;
		}

		auto ret = util::pool<ExprCall>(op.loc);
		iceAssert(op == TT::LParen);

		ret->args = parseCallArgumentList(st);
		ret->callee = lhs;

		return ret;
	}


	static Expr* parseIdentifier(State& st)
	{
		iceAssert(st.front() == TT::Identifier || st.front() == TT::UnicodeSymbol);
		std::string name = st.pop().str();

		auto ident = util::pool<Ident>(st.ploc(), name);

		//* we've modified our generic thing to be Foo!<...>, so there shouldn't
		//* be any ambiguity left. once we see '!' and '<' we know for sure.
		//? as long as we don't make '!' a postfix operator...
		//? for now, we'll leave in the try-and-restore mechanism.
		if(st.front() == TT::Exclamation && st.lookahead(1) == TT::LAngle)
		{
			bool fail = false;
			auto restore = st.getIndex();

			auto pams = parsePAMs(st, &fail);
			if(fail)
			{
				st.rewindTo(restore);
			}
			else
			{
				ident->mappings = pams;
				ident->loc.len += (st.ploc().col - st.getTokenList()[restore].loc.col) + 1;
			}
		}

		return ident;
	}



	static Expr* parsePostfixUnary(State& st, Expr* lhs, Token op)
	{
		if(op.type == TT::LParen)
		{
			return parseCall(st, lhs, op);
		}
		else if(op.type == TT::LSquare)
		{
			st.enterSubscript();
			defer(st.leaveSubscript());

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
					auto ret = util::pool<SliceOp>(op.loc);
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

				auto ret = util::pool<SubscriptOp>(op.loc);
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

				auto ret = util::pool<SliceOp>(op.loc);
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

				if(st.front() != TT::RSquare)
					slcend = parseExpr(st);

				//* note: looks duplicated and wrong, but rest assured it's fine. if we get arr[x:], that's valid.
				if(st.front() != TT::RSquare)
					expectedAfter(st.loc(), "']'", "expression for slice operation", st.front().str());

				st.eat();

				auto ret = util::pool<SliceOp>(op.loc);
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
		else if(auto it = st.postfixOps.find(op.str()); it != st.postfixOps.end())
		{
			// yay...?
			//* note: custom postfix ops are handled internally in the AST as normal unary ops.
			auto ret = util::pool<UnaryOp>(op.loc);

			ret->expr = lhs;
			ret->op = op.str();
			ret->isPostfix = true;

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

		auto ret = util::pool<AllocOp>(loc);

		if(st.front() == TT::Mutable)
			ret->isMutable = true, st.pop();

		ret->isRaw = raw;
		ret->allocTy = parseType(st);

		if(st.front() == TT::LParen)
		{
			st.pop();
			ret->args = parseCallArgumentList(st);

			if(ret->args.empty())
				info(st.loc(), "empty argument list in alloc expression () can be omitted");
		}


		if(st.front() == TT::LSquare)
		{
			st.eat();
			while(st.front() != TT::RSquare)
			{
				ret->counts.push_back(parseExpr(st));
				if(st.front() == TT::Comma)
					st.eat();

				else if(st.front() != TT::RSquare)
					expected(st.loc(), "',' or ')' in dimension list for alloc expression", st.front().str());
			}

			if(st.eat() != TT::RSquare)
				expectedAfter(st.ploc(), "']'", "'alloc(...)'", st.prev().str());

			if(ret->counts.empty())
				error(st.ploc(), "dimension list in 'alloc' cannot be empty");
		}

		if(st.front() == TT::LBrace)
		{
			if(raw) error(st.loc(), "initialisation body cannot be used with raw array allocations");

			// ok, get it
			ret->initBody = parseBracedBlock(st);
		}


		return ret;
	}

	DeallocOp* parseDealloc(State& st)
	{
		iceAssert(st.front() == TT::Dealloc);
		auto loc = st.eat().loc;

		auto ret = util::pool<DeallocOp>(loc);
		ret->expr = parseExpr(st);

		return ret;
	}

	DeferredStmt* parseDefer(State& st)
	{
		iceAssert(st.front() == TT::Defer);
		auto ret = util::pool<DeferredStmt>(st.eat().loc);

		if(st.front() == TT::LBrace)
			ret->actual = parseBracedBlock(st);

		else
			ret->actual = parseStmt(st);

		return ret;
	}

	SizeofOp* parseSizeof(State& st)
	{
		Token tok = st.eat();
		iceAssert(tok == TT::Sizeof);

		if(st.eat() != TT::LParen)
			expectedAfter(st.ploc(), "'('", "sizeof", st.prev().str());

		auto ret = util::pool<SizeofOp>(tok.loc);
		ret->expr = parseExpr(st);

		if(st.eat() != TT::RParen)
			expectedAfter(st.ploc(), "')'", "expression in sizeof", st.prev().str());

		return ret;
	}


	TypeidOp* parseTypeid(State& st)
	{
		Token tok = st.eat();
		iceAssert(tok == TT::Typeid);

		if(st.eat() != TT::LParen)
			expectedAfter(st.ploc(), "'('", "typeid", st.prev().str());

		auto ret = util::pool<TypeidOp>(tok.loc);
		ret->expr = parseExpr(st);

		if(st.eat() != TT::RParen)
			expectedAfter(st.ploc(), "')'", "expression in typeid", st.prev().str());

		return ret;
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
				case TT::Using:
				case TT::Union:
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
					error(st, "declarations are not expressions (%s)", tok.str());

				case TT::Dealloc:
					error(st, "deallocation statements are not expressions");

				case TT::Defer:
					error(st, "defer statements are not expressions");

				case TT::Return:
					error(st, "return statements are not expressions");

				case TT::Break:
					error(st, "break statements are not expressions");

				case TT::Continue:
					error(st, "continue statements are not expressions");

				case TT::If:
					error(st, "if statements are not expressions");

				case TT::Do:
				case TT::While:
				case TT::Loop:
				case TT::For:
					error(st, "loops are not expressions");





				case TT::Dollar:
					return parseDollarExpr(st);

				case TT::LParen:
					return parseParenthesised(st);

				case TT::Identifier:
				case TT::DoubleColon:
				case TT::UnicodeSymbol:
					return parseIdentifier(st);

				case TT::Caret:
					return parseCaretScopeExpr(st);

				case TT::Alloc:
					return parseAlloc(st, false);

				// case TT::Typeof:
				// 	return parseTypeof(ps);

				case TT::Typeid:
					return parseTypeid(st);

				case TT::Sizeof:
					return parseSizeof(st);

				case TT::Attr_Raw:
					st.pop();
					if(st.front() == TT::StringLiteral)
						return parseString(st, true);

					else if(st.front() == TT::LSquare)
						return parseArray(st, true);

					else if(st.front() == TT::Alloc)
						return parseAlloc(st, true);

					else
						expectedAfter(st, "one of string-literal, array, or alloc", "@raw", st.front().str());

				case TT::StringLiteral:
					return parseString(st, false);

				case TT::Number:
					return parseNumber(st);

				case TT::LSquare:
					return parseArray(st, false);

				// no point creating separate functions for these
				case TT::True:
					st.pop();
					return util::pool<LitBool>(tok.loc, true);

				case TT::False:
					st.pop();
					return util::pool<LitBool>(tok.loc, false);

				// nor for this
				case TT::Null:
					st.pop();
					return util::pool<LitNull>(tok.loc);


				case TT::LBrace:
					// parse it, but throw it away
					// warn(parseBracedBlock(st)->loc, "Anonymous blocks are ignored; to run, preface with 'do'");
					// return 0;
					unexpected(st, "block; to create a nested scope, use 'do { ... }'");

				default:
					error(tok.loc, "unexpected token '%s' (id = %d)", tok.str(), tok.type);
			}
		}

		return 0;
	}













}
























