// expr.cpp
// Copyright (c) 2014 - 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "parser.h"
#include "parser_internal.h"

#include "memorypool.h"

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
		auto stmt = parseStmt(st, /* allowExprs: */ false);
		if(auto fd = dcast(FuncDefn, stmt))
			fd->visibility = vis;

		else if(auto ffd = dcast(ForeignFuncDefn, stmt))
			ffd->visibility = vis;

		else if(auto vd = dcast(VarDefn, stmt))
			vd->visibility = vis;

		else if(auto td = dcast(TypeDefn, stmt))
			td->visibility = vis;

		else
			error(st, "access specifier cannot be applied to this statement");

		return stmt;
	}

	Stmt* parseStmt(State& st, bool allowExprs)
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
			auto attrs = parseAttributes(st);
			auto enforceAttrs = [&attrs](Stmt* ret, const AttribSet& allowed = AttribSet::of(attr::NONE)) -> Stmt* {

				using namespace attr;

				// there's probably a better way to do this, but bleugh
				if((attrs.flags & RAW) && !(allowed.flags & RAW))
					error(ret, "unsupported attribute '@raw' on %s", ret->readableName);

				if((attrs.flags & NO_MANGLE) && !(allowed.flags & NO_MANGLE))
					error(ret, "unsupported attribute '@nomangle' on %s", ret->readableName);

				if((attrs.flags & FN_ENTRYPOINT) && !(allowed.flags & FN_ENTRYPOINT))
					error(ret, "unsupported attribute '@entry' on %s", ret->readableName);

				// here let's check the arguments and stuff for default attributes.
				// note: due to poor API design on my part, if there is no attribute with that name then ::get()
				// returns an empty UA, which has a blank name -- so we check that instead.

				if(auto ua = attrs.get("@compiler_support"); !ua.name.empty() && ua.args.size() != 1)
					error(ret, "@compiler_support requires exactly one argument");

				// actually that's it

				ret->attrs = attrs;
				return ret;
			};

			// handle the things that are OK to appear anywhere first:
			tok = st.front();
			switch(tok.type)
			{
				case TT::Var:   [[fallthrough]];
				case TT::Val:
					return enforceAttrs(parseVariable(st), AttribSet::of(attr::NO_MANGLE));

				case TT::Func:
					return enforceAttrs(parseFunction(st), AttribSet::of(attr::NO_MANGLE | attr::FN_ENTRYPOINT));

				case TT::ForeignFunc:
					return enforceAttrs(parseForeignFunction(st));

				case TT::Public:    [[fallthrough]];
				case TT::Private:   [[fallthrough]];
				case TT::Internal:
					return enforceAttrs(parseStmtWithAccessSpec(st));

				case TT::Directive_If:
					return enforceAttrs(parseIfStmt(st));

				case TT::Directive_Run:
					return enforceAttrs(parseRunDirective(st));

				case TT::Union:
					return enforceAttrs(parseUnion(st, attrs.has(attr::RAW), /* nameless: */ false), AttribSet::of(attr::RAW));

				case TT::Struct:
					return enforceAttrs(parseStruct(st, /* nameless: */ false));

				case TT::Class:
					return enforceAttrs(parseClass(st));

				case TT::Enum:
					return enforceAttrs(parseEnum(st));

				case TT::Trait:
					return enforceAttrs(parseTrait(st));

				case TT::Static:
					return enforceAttrs(parseStaticDecl(st));

				case TT::Operator:
					return enforceAttrs(parseOperatorOverload(st));

				case TT::Using:
					return enforceAttrs(parseUsingStmt(st));

				case TT::Mutable:   [[fallthrough]];
				case TT::Virtual:   [[fallthrough]];
				case TT::Override:
					return enforceAttrs(checkMethodModifiers(false, false, false), AttribSet::of(attr::NO_MANGLE));

				case TT::Extension: [[fallthrough]];
				case TT::TypeAlias:
					error(st, "notsup");

				case TT::Else:
					error(st, "cannot have 'else' without preceeding 'if'");

				case TT::Namespace:
					error(st, "namespaces can only be defined at the top-level scope");

				case TT::Import:
					error(st, "all imports must happen at the top-level scope");

				case TT::Attr_Operator:
					error(st, "all operator declarations must happen at the top-level scope");

				case TT::Export:
					error(st, "export declaration must be the first non-comment line in the file");

				default:
					// do nothing.
					break;
			}


			// if we got here, then it wasn't any of those things.
			// we store it first, so we can give better error messages (after knowing what it is)
			// in the event that it wasn't allowed at top-level.


			Stmt* ret = 0;
			switch(tok.type)
			{
				case TT::If:
					ret = parseIfStmt(st);
					break;

				case TT::Else:
					error(st, "cannot have 'else' without preceeding 'if'");

				case TT::Return:
					ret = parseReturn(st);
					break;

				case TT::Do:    [[fallthrough]];
				case TT::While:
					ret = parseWhileLoop(st);
					break;

				case TT::For:
					ret = parseForLoop(st);
					break;

				case TT::Break:
					ret = parseBreak(st);
					break;

				case TT::Continue:
					ret = parseContinue(st);
					break;

				case TT::Dealloc:
					ret = parseDealloc(st);
					break;

				case TT::Defer:
					ret = parseDefer(st);
					break;

				default: {
					if(st.isInStructBody() && tok.type == TT::Identifier)
					{
						if(tok.str() == "init")
						{
							ret = parseInitFunction(st);
							break;
						}
						else if(tok.str() == "deinit")
						{
							ret = parseDeinitFunction(st);
							break;
						}
						else if(tok.str() == "copy" || tok.str() == "move")
						{
							ret = parseCopyOrMoveInitFunction(st, tok.str());
							break;
						}
					}

					// we want to error on invalid tokens first. so, we parse the expression regardless,
					// then if they're not allowed we error.
					auto expr = parseExpr(st);

					if(!allowExprs) error(expr, "expressions are not allowed at the top-level");
					else            ret = expr;

					break;
				}
			}

			iceAssert(ret);
			if(!st.isInFunctionBody() && !st.isInStructBody())
			{
				error(ret, "%s is not allowed at the top-level", ret->readableName);
			}
			else
			{
				return ret;
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

	static int tok_precedence(State& st, TokenType t)
	{
		switch(t)
		{
			/*
				! ACHTUNG !
				* DOCUMENT THIS SOMEWHERE PROPERLY!!! *

				due to how we handle identifiers and scope paths (foo::bar), function calls must have higher precedence
				than scope resolution.

				this might seem counter-intuitive (i should be resolving the complete identifier first, then calling it with
				()! why would it be any other way???), this is a sad fact of how the typechecker works.

				as it stands, identifiers are units; paths consist of multiple identifiers in a DotOp with the :: operator, which
				is left-associative. so for something like foo::bar::qux, it's ((foo)::(bar))::qux.

				to resolve a function call, eg. foo::bar::qux(), the DotOp is laid out as [foo::bar]::[qux()] (with [] for grouping),
				instead of the 'intuitive' [[foo::bar]::qux](). the reason for this design was the original rewrite goal of not
				relying on string manipulation in the compiler; having identifiers contain :: would have been counter to that
				goal.

				(note: currently we are forced to manipulate ::s in the pts->fir type converter!!)

				the current typechecker will thus first find a namespace 'foo', and within that a namespace 'bar', and within that
				a function 'qux'. this is opposed to finding a namespace 'foo', then 'bar', then an identifier 'qux', and leaving
				that to be resolved later.

				also, another potential issue is how we deal with references to functions (ie. function pointers). our resolver
				for ExprCall is strictly less advanced than that for a normal FunctionCall (for reasons i can't reCALL (lmao)), so
				we would prefer to return a FuncCall rather than an ExprCall.


				this model could be re-architected without a *major* rewrite, but it would be a non-trivial task and a considerable amount
				of work and debugging. for reference:

				1.  make Idents be able to refer to entire paths; just a datastructure change
				2.  make :: have higher precedence than (), to take advantage of (1)
				3.  parse ExprCall and FuncCall identically -- they should both just be a NewCall, with an Expr as the callee

				4.  in typechecking a NewCall, just call ->typecheck() on the LHS; the current implementation of Ident::typecheck
					returns an sst::VarRef, which has an sst::VarDefn field which we can use.

				4a. if the Defn was a VarDefn, they cannot overload, and we can just do what we currently do for ExprCall.
					if it was a function defn, then things get more complicated.

				5.  the Defn was a FuncDefn. currently Ident cannot return more than one Defn in the event of ambiguous results (eg.
					when overloading!), which means we are unable to properly do overload resolution! (duh) we need to make a mechanism
					for Ident to return a list of Defns.

					potential solution (A): make an sst::AmbiguousDefn struct that itself holds a list of Defns. the VarRef returned by
					Ident would then return that in the ->def field. this would only happen when the target is function; i presume we
					have existing mechanisms to detect invalid "overload" scenarios.

					back to (4), we should in theory be able to resolve functions from a list of defns.


				the problem with this is that while it might seem like a simple 5.5-step plan, a lot of the supporting resolver functions
				need to change, and effort is better spent elsewhere tbh

				for now we just stick to parsing () at higher precedence than ::.
			*/


			case TT::LParen:
				return 9001;    // very funny

			case TT::DoubleColon:
				return 5000;

			case TT::Period:
				return 1500;

			case TT::LSquare:
				return 1000;


			// unary !
			// unary +/-
			// bitwise ~
			// unary &
			// unary *
			// unary ... (splat)
			// ^^ all have 950.

			case TT::As:            [[fallthrough]];
			case TT::Is:
				return 900;

			case TT::DoublePlus:    [[fallthrough]];
			case TT::DoubleMinus:
				return 850;

			case TT::Asterisk:      [[fallthrough]];
			case TT::Divide:        [[fallthrough]];
			case TT::Percent:
				return 800;

			case TT::Plus:          [[fallthrough]];
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

			case TT::LAngle:        [[fallthrough]];
			case TT::RAngle:        [[fallthrough]];
			case TT::LessThanEquals:[[fallthrough]];
			case TT::GreaterEquals: [[fallthrough]];
			case TT::EqualsTo:      [[fallthrough]];
			case TT::NotEquals:
				return 500;

			case TT::Ellipsis:      [[fallthrough]];
			case TT::HalfOpenEllipsis:
				return 475;

			case TT::LogicalAnd:
				return 400;

			case TT::LogicalOr:
				return 350;

			case TT::Equal:         [[fallthrough]];
			case TT::PlusEq:        [[fallthrough]];
			case TT::MinusEq:       [[fallthrough]];
			case TT::MultiplyEq:    [[fallthrough]];
			case TT::DivideEq:      [[fallthrough]];
			case TT::ModEq:         [[fallthrough]];
			case TT::AmpersandEq:   [[fallthrough]];
			case TT::PipeEq:        [[fallthrough]];
			case TT::CaretEq:
				return 100;

			default:
				if(auto it = st.binaryOps.find(st.front().str()); it != st.binaryOps.end())
					return it->second.precedence;

				else if(it = st.postfixOps.find(st.front().str()); it != st.postfixOps.end())
					return it->second.precedence;

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

		return tok_precedence(st, st.front());
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


	Expr* parseCaretOrColonScopeExpr(State& st)
	{
		iceAssert(util::match(st.front(), TT::DoubleColon, TT::Caret));

		auto str = st.front().str();
		auto loc = st.loc();

		if(st.front() == TT::Caret)
		{
			st.eat();
			if(st.front() != TT::DoubleColon)
				expectedAfter(st.loc(), "'::'", "'^' in scope-path-specifier", st.front().str());
		}

		// ^::(^::((foo::bar)::qux))

		auto ident = util::pool<ast::Ident>(loc, str);
		return parseRhs(st, ident, tok_precedence(st, TT::DoubleColon));
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
		{
			SpanError::make(SimpleError::make(st.loc(), "expected ',' to begin a tuple, or a closing ')'"))->add(
				util::ESpan(opening.loc, "opening parenthesis was here")
			)->append(BareError::make(MsgType::Note, "named tuples are not supported"))->postAndQuit();
		}

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

	static FunctionCall* parseFunctionCall(State& st, const Location& loc, std::string name)
	{
		auto ret = util::pool<FunctionCall>(loc, name);

		st.skipWS();
		ret->args = parseCallArgumentList(st);

		return ret;
	}


	static Expr* parseCall(State& st, Expr* lhs, Token op)
	{
		if(Ident* id = dcast(Ident, lhs))
		{
			auto ret = parseFunctionCall(st, id->loc, id->name);
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
		iceAssert(util::match(st.front(), TT::Identifier, TT::UnicodeSymbol));
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

		if(raw)
			ret->attrs.set(attr::RAW);

		ret->allocTy = parseType(st);

		if(st.front() == TT::LParen)
		{
			auto leftloc = st.loc();

			st.pop();
			ret->args = parseCallArgumentList(st);

			if(ret->args.empty())
			{
				// parseCallArgumentList consumes the closing )
				auto tmp = Location::unionOf(leftloc, st.ploc());
				info(tmp, "empty argument list in alloc expression () can be omitted");
			}
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
				case TT::Trait:
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
				case TT::For:
					error(st, "loops are not expressions");





				case TT::Dollar:
					return parseDollarExpr(st);

				case TT::LParen:
					return parseParenthesised(st);

				case TT::Identifier:
				case TT::UnicodeSymbol:
					return parseIdentifier(st);

				case TT::Caret:
				case TT::DoubleColon:
					return parseCaretOrColonScopeExpr(st);

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
						return parseString(st, /* isRaw: */ true);

					else if(st.front() == TT::LSquare)
						return parseArray(st, /* isRaw: */ true);

					else if(st.front() == TT::Alloc)
						return parseAlloc(st, /* isRaw: */ true);

					else
						expectedAfter(st, "one of string-literal, array, or alloc", "'@raw' while parsing expression", st.front().str());

				case TT::StringLiteral:
					return parseString(st, false);

				case TT::Number:
					return parseNumber(st);

				case TT::LSquare:
					return parseArray(st, /* isRaw: */ false);

				case TT::CharacterLiteral:
					st.pop();
					return util::pool<LitChar>(tok.loc, static_cast<uint32_t>(tok.text[0]));

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

				case TT::Directive_Run:
					return parseRunDirective(st);

				case TT::LBrace:
					unexpected(st, "block; to create a nested scope, use 'do { ... }'");

				default:
					error(tok.loc, "unexpected token '%s' (id = %d)", tok.str(), tok.type);
			}
		}

		return 0;
	}













}
























