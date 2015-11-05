// Parser.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include <map>
#include <deque>
#include <cfloat>
#include <fstream>
#include <cassert>
#include <cinttypes>

#include "ast.h"
#include "parser.h"
#include "compiler.h"
#include "codegen.h"

using namespace Ast;


namespace Parser
{
	#define CreateAST_Raw(name, ...)		(new name (currentPos, ##__VA_ARGS__))
	#define CreateAST(name, tok, ...)		(new name (tok.pin, ##__VA_ARGS__))

	#define CreateASTPos(name, f, l, c, len, ...)	(new name (Parser::Pin(f, l, c, len), ##__VA_ARGS__))


	#define ATTR_STR_NOMANGLE			"nomangle"
	#define ATTR_STR_FORCEMANGLE		"forcemangle"
	#define ATTR_STR_NOAUTOINIT			"noinit"
	#define ATTR_STR_PACKEDSTRUCT		"packed"
	#define ATTR_STR_STRONG				"strong"
	#define ATTR_STR_RAW				"raw"
	#define ATTR_STR_OPERATOR			"operator"

	static ParserState* staticState;

	std::string getModuleName(std::string filename)
	{
		size_t lastdot = filename.find_last_of(".");
		std::string modname = (lastdot == std::string::npos ? filename : filename.substr(0, lastdot));

		size_t sep = modname.find_last_of("\\/");
		if(sep != std::string::npos)
			modname = modname.substr(sep + 1, modname.length() - sep - 1);

		return modname;
	}

	static bool checkHasMore(ParserState& ps)
	{
		return ps.tokens.size() > 0;
	}

	static bool isRightAssociativeOp(Token tok)
	{
		return false;
	}

	static bool isPostfixUnaryOperator(TType tt)
	{
		return (tt == TType::LSquare) || (tt == TType::DoublePlus) || (tt == TType::DoubleMinus);
	}

	static int getCurOpPrec(ParserState& ps)
	{
		// handle >>, >>=, <<, <<=.
		if(ps.tokens.size() > 1 && (ps.front().type == TType::LAngle || ps.front().type == TType::RAngle))
		{
			// check if the next one matches.
			if(ps.front().type == TType::LAngle && ps.tokens[1].type == TType::LAngle)
				return 160;

			else if(ps.front().type == TType::RAngle && ps.tokens[1].type == TType::RAngle)
				return 160;


			else if(ps.front().type == TType::LAngle && ps.tokens[1].type == TType::LessThanEquals)
				return 90;

			else if(ps.front().type == TType::RAngle && ps.tokens[1].type == TType::GreaterEquals)
				return 90;
		}

		switch(ps.front().type)
		{
			case TType::Comma:
				return ps.didHaveLeftParen ? 9001 : -1;	// lol x3

			case TType::As:
			case TType::Period:
				return 400;

			// array index: 120
			case TType::LSquare:
				return 310;

			case TType::DoublePlus:
			case TType::DoubleMinus:
				return 300;

			case TType::ShiftLeft:
			case TType::ShiftRight:
				iceAssert(0);	// note: handled above
				break;
				// return 160;

			case TType::Asterisk:
			case TType::Divide:
			case TType::Percent:
			case TType::Ampersand:
				return 160;

			case TType::Plus:
			case TType::Minus:
			case TType::Pipe:
				return 140;

			case TType::LAngle:
			case TType::RAngle:
			case TType::LessThanEquals:
			case TType::GreaterEquals:
			case TType::EqualsTo:
			case TType::NotEquals:
				return 130;

			case TType::LogicalAnd:
				return 120;

			case TType::LogicalOr:
				return 110;

			case TType::Equal:
			case TType::PlusEq:
			case TType::MinusEq:
			case TType::MultiplyEq:
			case TType::DivideEq:
			case TType::ModEq:
				return 90;


			case TType::ShiftLeftEq:
			case TType::ShiftRightEq:
				iceAssert(0);	// note: handled above.
				break;


			case TType::Identifier:
				if(ps.cgi->customOperatorMapRev.find(ps.front().text) != ps.cgi->customOperatorMapRev.end())
				{
					return ps.cgi->customOperatorMap[ps.cgi->customOperatorMapRev[ps.front().text]].second;
				}
				return -1;

			default:
				return -1;
		}
	}

	std::string arithmeticOpToString(Codegen::CodegenInstance* cgi, Ast::ArithmeticOp op)
	{
		switch(op)
		{
			case ArithmeticOp::Add:					return "+";
			case ArithmeticOp::Subtract:			return "-";
			case ArithmeticOp::Multiply:			return "*";
			case ArithmeticOp::Divide:				return "/";
			case ArithmeticOp::Modulo:				return "%";
			case ArithmeticOp::ShiftLeft:			return "<<";
			case ArithmeticOp::ShiftRight:			return ">>";
			case ArithmeticOp::Assign:				return "=";
			case ArithmeticOp::CmpLT:				return "<";
			case ArithmeticOp::CmpGT:				return ">";
			case ArithmeticOp::CmpLEq:				return "<=";
			case ArithmeticOp::CmpGEq:				return ">=";
			case ArithmeticOp::CmpEq:				return "==";
			case ArithmeticOp::CmpNEq:				return "!=";
			case ArithmeticOp::LogicalNot:			return "!";
			case ArithmeticOp::Plus:				return "+";
			case ArithmeticOp::Minus:				return "-";
			case ArithmeticOp::AddrOf:				return "&";
			case ArithmeticOp::Deref:				return "#";
			case ArithmeticOp::BitwiseAnd:			return "&";
			case ArithmeticOp::BitwiseOr:			return "|";
			case ArithmeticOp::BitwiseXor:			return "^";
			case ArithmeticOp::BitwiseNot:			return "~";
			case ArithmeticOp::LogicalAnd:			return "&&";
			case ArithmeticOp::LogicalOr:			return "||";
			case ArithmeticOp::Cast:				return "as";
			case ArithmeticOp::ForcedCast:			return "as!";
			case ArithmeticOp::PlusEquals:			return "+=";
			case ArithmeticOp::MinusEquals:			return "-=";
			case ArithmeticOp::MultiplyEquals:		return "*=";
			case ArithmeticOp::DivideEquals:		return "/=";
			case ArithmeticOp::ModEquals:			return "%=";
			case ArithmeticOp::ShiftLeftEquals:		return "<<=";
			case ArithmeticOp::ShiftRightEquals:	return ">>=";
			case ArithmeticOp::BitwiseAndEquals:	return "&=";
			case ArithmeticOp::BitwiseOrEquals:		return "|=";
			case ArithmeticOp::BitwiseXorEquals:	return "^=";
			case ArithmeticOp::MemberAccess:		return ".";
			case ArithmeticOp::ScopeResolution:		return "::";
			case ArithmeticOp::TupleSeparator:		return ",";
			case ArithmeticOp::Invalid:				parserError("Invalid arithmetic operator");

			default:								return cgi->customOperatorMap[op].first;
		}
	}

	static int64_t getIntegerValue(Token t)
	{
		iceAssert(t.type == TType::Integer);
		int base = 10;
		if(t.text.compare(0, 2, "0x") == 0)
			base = 16;

		return std::stoll(t.text, nullptr, base);
	}

	static double getDecimalValue(Token t)
	{
		return std::stod(t.text);
	}

	static const char* ReadableAttrNames[] =
	{
		"Invalid",
		"NoMangle",
		"Public",
		"Internal",
		"Private",
		"ForceMangle",
		"NoAutoInit",
		"Packed",
		"Strong",
		"Raw",
		"Override",
		"Commutative"
	};

	static uint64_t checkAndApplyAttributes(ParserState& ps, uint64_t allowed)
	{
		uint64_t disallowed = ~allowed;

		if(ps.curAttrib & disallowed)
		{

			int shifts = 0;
			while(((ps.curAttrib & disallowed) & 1) == 0)
				ps.curAttrib >>= 1, disallowed >>= 1, shifts++;

			if(shifts > 0)
				parserError("Invalid attribute '%s' for expression", ReadableAttrNames[shifts + 1]);
		}

		uint64_t ret = ps.curAttrib;
		ps.curAttrib = 0;
		return ret;
	}



	void parseAllCustomOperators(ParserState& ps, std::string filename, std::string curpath)
	{
		staticState = &ps;

		// split into lines
		std::string fullpath = Compiler::getFullPathOfFile(filename);
		ps.tokens = Compiler::getFileTokens(fullpath);

		ps.currentPos.file = new char[filename.length() + 1];
		strcpy(ps.currentPos.file, filename.c_str());

		ps.currentPos.line = 1;
		ps.currentPos.col = 1;


		// todo: hacks
		ps.isParsingStruct = 0;
		ps.didHaveLeftParen = 0;
		ps.currentOpPrec = 0;

		ps.skipNewline();

		// hackjob... kinda.
		auto findOperators = [&](ParserState& ps) {

			int curPrec = 0;
			while(ps.tokens.size() > 0)
			{
				Token t = ps.front();
				ps.pop_front();

				if(t.type == TType::Import)
				{
					// hack: parseImport expects front token to be "import"
					ps.tokens.push_front(t);

					Import* imp = parseImport(ps);
					std::string file = Compiler::resolveImport(imp, Compiler::getFullPathOfFile(filename));

					if(!ps.visited[file])
					{
						ps.visited[file] = true;

						ParserState fakePs(ps.cgi);
						parseAllCustomOperators(fakePs, file, curpath);
					}
				}
				else if(t.type == TType::At)
				{
					Token attr = ps.front();
					ps.pop_front();

					iceAssert(attr.type == TType::Identifier || attr.text == "public"
						|| attr.text == "private" || attr.text == "internal");

					if(attr.text == ATTR_STR_OPERATOR)
					{
						ps.skipNewline();
						if(ps.front().type != TType::LSquare)
							parserError(ps, ps.front(), "Expected '[' after @operator");

						ps.pop_front();
						ps.skipNewline();

						Token num = ps.front();
						ps.pop_front();
						ps.skipNewline();





						// todo: a bit messy
						if(num.type == TType::Identifier)
						{
							// skip.
							if(ps.front().type == TType::RSquare)
							{
								ps.pop_front();
								curPrec = 0;
								continue; // break out of the loopy
							}
							else if(ps.front().type == TType::Comma)
							{
								ps.pop_front();
								num = ps.front();
							}
							else
							{
								parserError(ps, ps.front(), "Expected either ']' or ',' after identifier in @operator");
							}
						}



						if(num.type != TType::Integer)
							parserError(ps, num, "Expected integer as first attribute within @operator[]");

						curPrec = std::stod(num.text);
						if(curPrec <= 0)
							parserError(ps, num, "Precedence must be greater than 0");

						ps.skipNewline();


						// Commutative
						if(ps.front().type == TType::Comma)
						{
							ps.pop_front();
							if(ps.eat().type != TType::Identifier)
								parserError(ps, ps.front(), "Expected identifier after comma");
						}


						if(ps.front().type != TType::RSquare)
							parserError(ps, ps.front(), "Expected closing ']'");


						ps.pop_front();
						ps.skipNewline();
					}
				}
				else if(t.type == TType::Identifier && t.text == "operator")
				{
					ps.skipNewline();
					Token op = ps.front();

					if(op.type == TType::Identifier)
					{
						size_t opNum = ps.cgi->customOperatorMap.size();

						if(curPrec <= 0)
							parserError(ps, t, "Custom operators must have a precedence, use @operator[x]");

						// check if it exists.
						if(ps.cgi->customOperatorMapRev.find(op.text) == ps.cgi->customOperatorMapRev.end())
						{
							ps.cgi->customOperatorMap[(ArithmeticOp) ((size_t) ArithmeticOp::UserDefined + opNum)] = { op.text, curPrec };
							ps.cgi->customOperatorMapRev[op.text] = (ArithmeticOp) ((size_t) ArithmeticOp::UserDefined + opNum);
						}
						else
						{
							ArithmeticOp ao = ps.cgi->customOperatorMapRev[op.text];
							auto pair = ps.cgi->customOperatorMap[ao];

							if(pair.second != curPrec)
							{
								parserWarn(op, "Operator '%s' was previously defined with a different precedence (%d). Due to the way"
									" the flax compiler is engineered, all custom operators using the same identifier will be bound to the"
									" first precedence defined.", pair.first.c_str(), pair.second);
							}
						}

						curPrec = 0;
					}
				}
				else if(curPrec > 0)
				{
					parserError(ps, ps.front(), "@operator can only be applied to operators (%s)", ps.front().text.c_str());
				}
			}

		};

		findOperators(ps);
	}

	Root* Parse(ParserState& ps, std::string filename)
	{
		Token t;

		// restore this, so we don't have to read the file again
		ps.tokens = Compiler::getFileTokens(filename);

		ps.rootNode = new Root();

		ps.currentPos.file = new char[filename.length() + 1];
		strcpy(ps.currentPos.file, filename.c_str());

		ps.currentPos.line = 1;
		ps.currentPos.col = 1;


		// todo: hacks
		ps.isParsingStruct = 0;
		ps.didHaveLeftParen = 0;
		ps.currentOpPrec = 0;

		ps.skipNewline();

		staticState = &ps;

		parseAll(ps);
		return ps.rootNode;
	}


	// this only handles the topmost level.
	void parseAll(ParserState& ps)
	{
		if(ps.tokens.size() == 0)
			return;

		Token tok;
		while(ps.tokens.size() > 0 && (tok = ps.front()).text.length() > 0)
		{
			switch(tok.type)
			{
				case TType::Func:
					ps.rootNode->topLevelExpressions.push_back(parseFunc(ps));
					break;

				case TType::Import:
					ps.rootNode->topLevelExpressions.push_back(parseImport(ps));
					break;

				case TType::ForeignFunc:
					ps.rootNode->topLevelExpressions.push_back(parseForeignFunc(ps));
					break;

				case TType::Struct:
					ps.rootNode->topLevelExpressions.push_back(parseStruct(ps));
					break;

				case TType::Class:
					ps.rootNode->topLevelExpressions.push_back(parseClass(ps));
					break;

				case TType::Enum:
					ps.rootNode->topLevelExpressions.push_back(parseEnum(ps));
					break;

				case TType::Extension:
					ps.rootNode->topLevelExpressions.push_back(parseExtension(ps));
					break;

				case TType::Var:
				case TType::Val:
					ps.rootNode->topLevelExpressions.push_back(parseVarDecl(ps));
					break;

				// shit you just skip
				case TType::NewLine:
					ps.currentPos.line++;
					[[clang::fallthrough]];

				case TType::Comment:
				case TType::Semicolon:
					ps.pop_front();
					break;

				case TType::TypeAlias:
					ps.rootNode->topLevelExpressions.push_back(parseTypeAlias(ps));
					break;

				case TType::Namespace:
					ps.rootNode->topLevelExpressions.push_back(parseNamespace(ps));
					break;

				case TType::Private:
					ps.eat();
					ps.curAttrib |= Attr_VisPrivate;
					break;

				case TType::Internal:
					ps.eat();
					ps.curAttrib |= Attr_VisInternal;
					break;

				case TType::Public:
					ps.eat();
					ps.curAttrib |= Attr_VisPublic;
					break;

				case TType::Override:
					ps.eat();
					ps.curAttrib |= Attr_Override;
					break;

				case TType::At:
					parseAttribute(ps);
					break;

				case TType::Identifier:
					if(tok.text == "operator")
					{
						ps.rootNode->topLevelExpressions.push_back(parseOpOverload(ps));
						break;
					}
					[[clang::fallthrough]];

				default:	// wip: skip shit we don't know/care about for now
					parserError("Unknown token '%s'", tok.text.c_str());
			}
		}
	}

	Expr* parsePrimary(ParserState& ps)
	{
		if(ps.tokens.size() == 0)
			return nullptr;

		Token tok;
		while((tok = ps.front()).text.length() > 0)
		{
			switch(tok.type)
			{
				case TType::Var:
				case TType::Val:
					return parseVarDecl(ps);

				case TType::Func:
					return parseFunc(ps);

				case TType::ForeignFunc:
					return parseForeignFunc(ps);

				case TType::LParen:
					return parseParenthesised(ps);

				case TType::Identifier:
					if(tok.text == "init")
						return parseInitFunc(ps);

					else if(tok.text == "operator")
						return parseOpOverload(ps);

					return parseIdExpr(ps);

				case TType::Static:
					return parseStaticDecl(ps);

				case TType::Alloc:
					return parseAlloc(ps);

				case TType::Dealloc:
					return parseDealloc(ps);

				case TType::Struct:
					return parseStruct(ps);

				case TType::Class:
					return parseClass(ps);

				case TType::Enum:
					return parseEnum(ps);

				case TType::Defer:
					return parseDefer(ps);

				case TType::Typeof:
					return parseTypeof(ps);

				case TType::Extension:
					return parseExtension(ps);

				case TType::At:
					parseAttribute(ps);
					return parsePrimary(ps);

				case TType::StringLiteral:
					return parseStringLiteral(ps);

				case TType::Integer:
				case TType::Decimal:
					return parseNumber(ps);

				case TType::LSquare:
					return parseArrayLiteral(ps);

				case TType::Return:
					return parseReturn(ps);

				case TType::Break:
					return parseBreak(ps);

				case TType::Continue:
					return parseContinue(ps);

				case TType::If:
					return parseIf(ps);

				// since both have the same kind of AST node, parseWhile can handle both
				case TType::Do:
				case TType::While:
				case TType::Loop:
					return parseWhile(ps);

				case TType::For:
					return parseFor(ps);

				// shit you just skip
				case TType::NewLine:
					ps.currentPos.line++;
					[[clang::fallthrough]];

				case TType::Comment:
				case TType::Semicolon:
					ps.eat();
					return CreateAST(DummyExpr, tok);

				case TType::TypeAlias:
					return parseTypeAlias(ps);

				case TType::Namespace:
					return parseNamespace(ps);

				// no point creating separate functions for these
				case TType::True:
					ps.pop_front();
					return CreateAST(BoolVal, tok, true);

				case TType::False:
					ps.pop_front();
					return CreateAST(BoolVal, tok, false);



				// attributes-as-keywords
				// stored as attributes in the AST, but parsed as keywords by the parser.
				case TType::Private:
					ps.eat();
					ps.curAttrib |= Attr_VisPrivate;
					return parsePrimary(ps);

				case TType::Internal:
					ps.eat();
					ps.curAttrib |= Attr_VisInternal;
					return parsePrimary(ps);

				case TType::Public:
					ps.eat();
					ps.curAttrib |= Attr_VisPublic;
					return parsePrimary(ps);

				case TType::Override:
					ps.eat();
					ps.curAttrib |= Attr_Override;
					return parsePrimary(ps);

				case TType::LBrace:
					parserWarn("Anonymous blocks are ignored; to run, preface with 'do'");
					parseBracedBlock(ps);		// parse it, but throw it away
					return CreateAST(DummyExpr, ps.front());

				default:
					parserError("Unexpected token '%s'\n", tok.text.c_str());
			}
		}

		return nullptr;
	}











	Expr* parseUnary(ParserState& ps)
	{
		Token tk = ps.front();

		// check for unary shit
		ArithmeticOp op = ArithmeticOp::Invalid;

		if(tk.type == TType::Exclamation)		op = ArithmeticOp::LogicalNot;
		else if(tk.type == TType::Plus)			op = ArithmeticOp::Plus;
		else if(tk.type == TType::Minus)			op = ArithmeticOp::Minus;
		else if(tk.type == TType::Tilde)			op = ArithmeticOp::BitwiseNot;
		else if(tk.type == TType::Pound)			op = ArithmeticOp::Deref;
		else if(tk.type == TType::Ampersand)		op = ArithmeticOp::AddrOf;

		if(op != ArithmeticOp::Invalid)
		{
			ps.eat();
			Expr* un = parseUnary(ps);

			return CreateASTPos(UnaryOp, tk.pin.file, tk.pin.line, tk.pin.col, tk.pin.len + un->pin.len, op, un);
		}

		return parsePrimary(ps);
	}








	Expr* parseStaticDecl(ParserState& ps)
	{
		iceAssert(ps.front().type == TType::Static);
		if(!ps.isParsingStruct)
			parserError("Static declarations are only allowed inside struct definitions");

		ps.eat();
		if(ps.front().type == TType::Func)
		{
			Func* ret = parseFunc(ps);
			ret->decl->isStatic = true;
			return ret;
		}
		else if(ps.front().type == TType::Var || ps.front().type == TType::Val)
		{
			// static var.
			VarDecl* ret = parseVarDecl(ps);
			ret->isStatic = true;
			return ret;
		}
		else
		{
			parserError("Invaild static expression '%s'", ps.front().text.c_str());
		}
	}


	FuncDecl* parseFuncDecl(ParserState& ps)
	{
		// todo: better things? it's right now mostly hacks.
		if(ps.front().text != "init" && ps.front().text.find("operator") != 0)
			iceAssert(ps.eat().type == TType::Func);

		if(ps.front().type != TType::Identifier)
			parserError("Expected identifier, but got token of type %d", ps.front().type);

		Token func_id = ps.eat();
		std::string id = func_id.text;

		std::deque<std::string> genericTypes;

		// expect a left bracket
		Token paren = ps.eat();
		if(paren.type != TType::LParen && paren.type != TType::LAngle)
		{
			parserError("Expected '(' in function declaration, got '%s'", paren.text.c_str());
		}
		else if(paren.type == TType::LAngle)
		{
			// todo: handle parsing nested generics -- << >> would parse as '<<' and '>>', not '<' '<' and '>' '>'.
			Expr* inner = parseType(ps);
			iceAssert(inner->type.isLiteral);

			genericTypes.push_back(inner->type.strType);

			Token angleOrComma = ps.eat();
			if(angleOrComma.type == TType::Comma)
			{
				// parse more.
				Token tok;
				while(true)
				{
					Expr* gtype = parseType(ps);
					iceAssert(gtype->type.isLiteral);

					genericTypes.push_back(gtype->type.strType);

					tok = ps.eat();
					if(tok.type == TType::Comma)		continue;
					else if(tok.type == TType::RAngle)	break;
					else								parserError("Expected '>' or ','");
				}
			}
			else if(angleOrComma.type != TType::RAngle)
				parserError("Expected '>' or ','");

			ps.eat();
		}

		bool isVA = false;
		// get the parameter list
		// expect an identifer, colon, type
		std::deque<VarDecl*> params;
		std::map<std::string, VarDecl*> nameCheck;

		while(ps.tokens.size() > 0 && ps.front().type != TType::RParen)
		{
			Token tok_id;
			bool immutable = true;
			if((tok_id = ps.eat()).type != TType::Identifier)
			{
				if(tok_id.type == TType::Elipsis)
				{
					isVA = true;
					if(ps.front().type != TType::RParen)
						parserError("Vararg declaration must be last in the function declaration");

					break;
				}
				else if(tok_id.type == TType::Var)
				{
					immutable = false;
					tok_id = ps.eat();
				}
				else if(tok_id.type == TType::Val)
				{
					immutable = false;
					tok_id = ps.eat();
					parserWarn("Function parameters are immutable by default, 'val' is redundant");
				}
				else
				{
					parserError("Expected identifier");
				}
			}

			std::string id = tok_id.text;
			VarDecl* v = CreateAST(VarDecl, tok_id, id, immutable);

			// expect a colon
			if(ps.eat().type != TType::Colon)
				parserError("Expected ':' followed by a type");

			Expr* ctype = parseType(ps);
			v->type = ctype->type;
			delete ctype;		// cleanup

			if(!nameCheck[v->name])
			{
				params.push_back(v);
				nameCheck[v->name] = v;
			}
			else
			{
				parserError("Redeclared variable '%s' in argument list", v->name.c_str());
			}

			if(ps.front().type == TType::Comma)
				ps.eat();
		}

		// consume the closing paren
		ps.eat();

		// get return type.
		std::string ret;
		if(checkHasMore(ps) && ps.front().type == TType::Arrow)
		{
			ps.eat();
			Expr* ctype = parseType(ps);
			ret = ctype->type.strType;
			delete ctype;

			if(ret == "Void")
				parserWarn("Explicitly specifying 'Void' as the return type is redundant");
		}
		else
		{
			ret = "Void";
		}

		ps.skipNewline();
		FuncDecl* f = CreateAST(FuncDecl, func_id, id, params, ret);
		f->attribs = checkAndApplyAttributes(ps, Attr_VisPublic | Attr_VisInternal | Attr_VisPrivate |
			Attr_NoMangle | Attr_ForceMangle | Attr_Override);

		f->hasVarArg = isVA;
		f->genericTypes = genericTypes;

		return f;
	}

	ForeignFuncDecl* parseForeignFunc(ParserState& ps)
	{
		Token func = ps.front();
		iceAssert(func.type == TType::ForeignFunc);
		ps.eat();

		FFIType ffitype = FFIType::C;

		// check for specifying the type
		if(ps.front().type == TType::LParen)
		{
			ps.eat();
			if(ps.front().type != TType::Identifier)
				parserError("Expected type of external function, either C (default) or Cpp");

			Token ftype = ps.eat();
			std::string lftype = ftype.text;
			std::transform(lftype.begin(), lftype.end(), lftype.begin(), ::tolower);

			if(lftype == "c")			ffitype = FFIType::C;
			else if(lftype == "cpp")	ffitype = FFIType::Cpp;
			else						parserError("Unknown FFI type '%s'", ftype.text.c_str());

			if(ps.eat().type != TType::RParen)
				parserError("Expected ')'");
		}



		FuncDecl* decl = parseFuncDecl(ps);
		decl->isFFI = true;
		decl->ffiType = ffitype;

		return CreateAST(ForeignFuncDecl, func, decl);
	}

	BracedBlock* parseBracedBlock(ParserState& ps)
	{
		Token tok_cls = ps.eat();
		BracedBlock* c = CreateAST(BracedBlock, tok_cls);

		// make sure the first token is a left brace.
		if(tok_cls.type != TType::LBrace)
			parserError("Expected '{' to begin a block, found '%s'!", tok_cls.text.c_str());


		std::deque<DeferredExpr*> defers;

		// get the stuff inside.
		while(ps.tokens.size() > 0 && ps.front().type != TType::RBrace)
		{
			Expr* e = parseExpr(ps);
			DeferredExpr* d = nullptr;

			if((d = dynamic_cast<DeferredExpr*>(e)))
			{
				defers.push_front(d);
			}
			else
			{
				c->statements.push_back(e);
			}

			ps.skipNewline();
		}

		if(ps.eat().type != TType::RBrace)
			parserError("Expected '}'");



		for(auto d : defers)
		{
			c->deferredStatements.push_back(d);
		}

		return c;
	}

	Func* parseFunc(ParserState& ps)
	{
		Token front = ps.front();
		FuncDecl* decl = parseFuncDecl(ps);

		auto ret = CreateAST(Func, front, decl, parseBracedBlock(ps));

		return ret;
	}


	Expr* parseInitFunc(ParserState& ps)
	{
		Token front = ps.front();
		iceAssert(front.text == "init");

		// we need to disambiguate between calling the init() function, and defining an init() function
		// to do this, we can loop through the tokens (without consuming) until we find the closing ')'
		// then see if the token following that is a '{'. if so, it's a declaration, if not it's a call

		if(ps.tokens.size() < 3)
			parserError("Unexpected end of input");

		else if(ps.tokens.size() > 3 && ps.tokens[1].type != TType::LParen)
			parserError("Expected '(' for either function call or declaration");

		int parenLevel = 0;
		bool foundBrace = false;
		for(size_t i = 1; i < ps.tokens.size(); i++)
		{
			if(ps.tokens[i].type == TType::LParen)
			{
				parenLevel++;
			}
			else if(ps.tokens[i].type == TType::RParen)
			{
				parenLevel--;
				if(parenLevel == 0)
				{
					// look through each until we find a brace
					for(size_t k = i + 1; k < ps.tokens.size() && !foundBrace; k++)
					{
						if(ps.tokens[k].type == TType::Comment || ps.tokens[k].type == TType::NewLine)
							continue;

						else if(ps.tokens[k].type == TType::LBrace)
							foundBrace = true;

						else
							break;
					}

					break;
				}
			}
		}

		if(foundBrace)
		{
			// found a brace, it's a decl
			FuncDecl* decl = parseFuncDecl(ps);
			return CreateAST(Func, front, decl, parseBracedBlock(ps));
		}
		else
		{
			// no brace, it's a call
			// eat the "init" token
			ps.eat();
			return parseFuncCall(ps, "init");
		}
	}












	Expr* parseType(ParserState& ps)
	{
		Token tmp = ps.eat();

		if(tmp.type == TType::Identifier)
		{
			std::string baseType = tmp.text;

			// parse until we get a non-identifier and non-scoperes
			if(ps.tokens.size() > 0)
			{
				bool expectingScope = true;
				Token t = ps.front();
				while(t.text.length() > 0)
				{
					if((t.type == TType::DoubleColon || t.type == TType::Period) && expectingScope)
					{
						baseType += "::";
						expectingScope = false;
					}
					else if(t.type == TType::Identifier && !expectingScope)
					{
						baseType += t.text;
						expectingScope = true;
					}
					else
					{
						break;
					}

					ps.eat();
					t = ps.front();
				}
			}


			if(ps.front().type == TType::LAngle)
			{
				ps.eat();
				baseType += "<";

				while(true)
				{
					Expr* e = parseType(ps);
					baseType += e->type.strType;

					if(ps.front().type == TType::Comma)
					{
						baseType += ",";
						ps.eat();
					}
					else if(ps.front().type == TType::RAngle)
					{
						break;
					}
					else
					{
						parserError("Unexpected token %s in generic type list", ps.front().text.c_str());
					}
				}

				iceAssert(ps.eat().type == TType::RAngle);
				baseType += ">";
			}








			std::string ptrAppend = "";
			if(ps.tokens.size() > 0 && (ps.front().type == TType::Ptr || ps.front().type == TType::Asterisk))
			{
				while(ps.front().type == TType::Ptr || ps.front().type == TType::Asterisk)
					ps.eat(), ptrAppend += "*";
			}

			// check if the next token is a '['.
			while(ps.front().type == TType::LSquare)
			{
				ps.eat();

				// this parses multi-dim array types.
				Token n = ps.eat();
				if(n.type != TType::Integer)
					parserError("Expected integer size for fixed-length array");

				std::string dims = "[" + n.text + "]";

				n = ps.eat();
				if(n.type != TType::RSquare)
					parserError("Expected ']', have %s", n.text.c_str());

				ptrAppend += dims;
			}

			std::string ret = baseType + ptrAppend;
			Expr* ct = CreateAST(DummyExpr, tmp);
			ct->type.isLiteral = true;
			ct->type.strType = ret;

			return ct;
		}
		else if(tmp.type == TType::Typeof)
		{
			parserError("enotsup");

			Expr* ct = CreateAST(DummyExpr, tmp);
			ct->type.isLiteral = false;
			ct->type.strType = "__internal_error__";

			ct->type.type = parseExpr(ps);
			return ct;
		}
		else if(tmp.type == TType::LParen)
		{
			// tuple as a type.
			int parens = 1;

			std::string final = "(";
			while(parens > 0)
			{
				Token front = ps.eat();
				if(front.type == TType::Identifier)
				{
					// another hack: re-insert.
					ps.tokens.push_front(front);

					Expr* de = parseType(ps);
					final += de->type.strType;
					delete de;
				}
				else if(front.type == TType::Comma)
				{
					final += ",";
				}
				else if(front.type == TType::LParen)
				{
					final += "(";
					parens++;
				}

				else if(front.type == TType::RParen)
				{
					final += ")";
					parens--;
				}
			}

			Expr* ct = CreateAST(DummyExpr, tmp);
			ct->type.isLiteral = true;
			ct->type.strType = final;

			return ct;
		}
		else if(tmp.type == TType::LSquare)
		{
			// variable-sized array.
			// declared as pointers, basically.

			Expr* _dm = parseType(ps);
			iceAssert(_dm->type.isLiteral);

			DummyExpr* dm = CreateAST(DummyExpr, tmp);
			dm->type.isLiteral = true;
			dm->type.strType = "[" + _dm->type.strType + "]";

			Token next = ps.eat();
			if(next.type != TType::RSquare)
				parserError("Expected ']' after array type declaration.");

			return dm;
		}
		else
		{
			parserError("Expected type for variable declaration, got %s", tmp.text.c_str());
		}
	}

	VarDecl* parseVarDecl(ParserState& ps)
	{
		iceAssert(ps.front().type == TType::Var || ps.front().type == TType::Val);

		bool immutable = ps.front().type == TType::Val;
		uint64_t attribs = checkAndApplyAttributes(ps, Attr_NoAutoInit | Attr_VisPublic | Attr_VisInternal | Attr_VisPrivate | Attr_Override);

		ps.eat();

		// get the identifier.
		Token tok_id;
		if((tok_id = ps.eat()).type != TType::Identifier)
			parserError("Expected identifier for variable declaration.");

		std::string id = tok_id.text;
		VarDecl* v = CreateAST(VarDecl, tok_id, id, immutable);
		v->disableAutoInit = attribs & Attr_NoAutoInit;
		v->attribs = attribs;

		// check the type.
		Token colon = ps.eat();
		if(colon.type == TType::Colon)
		{
			Expr* ctype = parseType(ps);
			v->type = ctype->type;

			delete ctype;	// cleanup

			if(ps.front().type == TType::LParen)
			{
				// this form:
				// var foo: String("bla")

				// since parseFuncCall is actually built for this kind of hack (like with the init() thing)
				// it's easy.
				v->initVal = parseFuncCall(ps, v->type.strType);
			}
			else if(ps.front().type == TType::LBrace)
			{
				if(!ps.isParsingStruct)
					parserError("Computed properties can only be declared inside structs");

				// computed property, getting and setting

				// eat the brace, skip whitespace
				ComputedProperty* cprop = CreateAST(ComputedProperty, tok_id, id);
				ps.eat();

				cprop->type = v->type;
				cprop->attribs = v->attribs;
				delete v;

				bool didGetter = false;
				bool didSetter = false;
				for(int i = 0; i < 2; i++)
				{
					if(ps.front().type == TType::Get)
					{
						if(didGetter)
							parserError("Only one getter is allowed per computed property");

						didGetter = true;

						// parse a braced block.
						ps.eat();
						if(ps.front().type != TType::LBrace)
							parserError("Expected '{' after 'get'");

						cprop->getter = parseBracedBlock(ps);
					}
					else if(ps.front().type == TType::Set)
					{
						if(didSetter)
							parserError("Only one getter is allowed per computed property");

						didSetter = true;

						ps.eat();
						std::string setValName = "newValue";

						// see if we have parentheses
						if(ps.front().type == TType::LParen)
						{
							ps.eat();
							if(ps.front().type != TType::Identifier)
								parserError("Expected identifier for custom setter argument name");

							setValName = ps.eat().text;

							if(ps.eat().type != TType::RParen)
								parserError("Expected closing ')'");
						}

						cprop->setter = parseBracedBlock(ps);
						cprop->setterArgName = setValName;
					}
					else if(ps.front().type == TType::RBrace)
					{
						break;
					}
					else
					{
						// implicit read-only, 'get' not required
						// there's no set, so make i = 1 so we error on extra bits
						i = 1;

						// insert a dummy brace
						Token dummy;
						dummy.type = TType::LBrace;
						dummy.text = "{";

						ps.tokens.push_front(dummy);
						cprop->getter = parseBracedBlock(ps);


						// lol, another hack
						dummy.type = TType::RBrace;
						dummy.text = "}";
						ps.tokens.push_front(dummy);
					}
				}

				if(ps.eat().type != TType::RBrace)
					parserError("Expected closing '}'");

				return cprop;
			}
		}
		else if(colon.type == TType::Equal)
		{
			v->type = "Inferred";

			// make sure the init value parser below works, push the colon back onto the stack
			ps.tokens.push_front(colon);
		}
		else
		{
			parserError("Variable declaration without type requires initialiser for type inference");
		}

		if(!v->initVal)
		{
			if(ps.front().type == TType::Equal)
			{
				// we do
				ps.eat();

				v->initVal = parseExpr(ps);
				if(!v->initVal)
					parserError("Invalid initialiser for variable '%s'", v->name.c_str());
			}
			else if(immutable)
			{
				parserError("Constant variables require an initialiser at the declaration site");
			}
		}

		// if we got here, we're a normal variable.
		if(v->attribs & Attr_Override)
			parserError("'override' can only be used with a var inside a class, tried to override var in struct");

		return v;
	}

	Tuple* parseTuple(ParserState& ps, Ast::Expr* lhs)
	{
		iceAssert(lhs);

		Token first = ps.front();
		std::vector<Expr*> values;


		values.push_back(lhs);

		Token t = ps.front();
		while(true)
		{
			values.push_back(parseExpr(ps));
			if(ps.front().type == TType::RParen)
				break;

			else if(ps.front().type == TType::Comma)
				ps.eat();

			t = ps.front();
		}

		// leave the last rparen
		iceAssert(ps.front().type == TType::RParen);
		// ps.eat();

		return CreateAST(Tuple, first, values);
	}

	Expr* parseParenthesised(ParserState& ps)
	{
		iceAssert(ps.eat().type == TType::LParen);
		ps.didHaveLeftParen = true;
		Expr* within = parseExpr(ps);

		iceAssert(ps.front().type == TType::RParen);
		ps.eat();

		ps.didHaveLeftParen = false;
		return within;
	}

	Expr* parseExpr(ParserState& ps)
	{
		Expr* lhs = parseUnary(ps);
		if(!lhs)
			return nullptr;

		return parseRhs(ps, lhs, 0);
	}

	static Expr* parsePostfixUnaryOp(ParserState& ps, Token tok, Expr* curLhs)
	{
		// do something! quickly!

		// get the type of op.
		// prec: array index: 120

		// std::deque<Expr*> args;
		// PostfixUnaryOp::Kind k;

		Token top = tok;
		Expr* newlhs = 0;
		if(top.type == TType::LSquare)
		{
			// parse the inside expression
			Expr* inside = parseExpr(ps);
			if(ps.eat().type != TType::RSquare)
				parserError("Expected ']' after '[' for array index");

			newlhs = CreateAST(ArrayIndex, top, curLhs, inside);
		}
		else
		{
			iceAssert(false);
		}

		return newlhs;
	}

	Expr* parseRhs(ParserState& ps, Expr* lhs, int prio)
	{
		while(true)
		{
			int prec = getCurOpPrec(ps);
			if(prec < prio && !isRightAssociativeOp(ps.front()))
				return lhs;


			// we don't really need to check, because if it's botched we'll have returned due to -1 < everything

			Token tok_op = ps.eat();
			Token next1 = ps.front();

			ArithmeticOp op = ArithmeticOp::Invalid;
			if(tok_op.type == TType::LAngle || tok_op.type == TType::RAngle)
			{
				// check if the next one matches.
				if(tok_op.type == TType::LAngle)
				{
					if(next1.type == TType::LAngle)
					{
						// < <
						op = ArithmeticOp::ShiftLeft;
						ps.eat();
					}
					else if(next1.type == TType::LessThanEquals)
					{
						// < <=
						op = ArithmeticOp::ShiftLeftEquals;
						ps.eat();
					}
				}
				else if(tok_op.type == TType::RAngle)
				{
					if(next1.type == TType::RAngle)
					{
						// > >
						op = ArithmeticOp::ShiftRight;
						ps.eat();
					}
					else if(next1.type == TType::GreaterEquals)
					{
						// > >=
						op = ArithmeticOp::ShiftRightEquals;
						ps.eat();
					}
				}
			}
			else if(tok_op.type == TType::Comma && ps.didHaveLeftParen)
			{
				ps.didHaveLeftParen = false;
				return parseTuple(ps, lhs);
			}
			else if(isPostfixUnaryOperator(tok_op.type))
			{
				lhs = parsePostfixUnaryOp(ps, tok_op, lhs);
				continue;
			}



			if(op == ArithmeticOp::Invalid)
			{
				switch(tok_op.type)
				{
					case TType::Plus:			op = ArithmeticOp::Add;					break;
					case TType::Minus:			op = ArithmeticOp::Subtract;			break;
					case TType::Asterisk:		op = ArithmeticOp::Multiply;			break;
					case TType::Divide:			op = ArithmeticOp::Divide;				break;
					case TType::Percent:		op = ArithmeticOp::Modulo;				break;
					case TType::ShiftLeft:		op = ArithmeticOp::ShiftLeft;			break;
					case TType::ShiftRight:		op = ArithmeticOp::ShiftRight;			break;
					case TType::Equal:			op = ArithmeticOp::Assign;				break;

					case TType::LAngle:			op = ArithmeticOp::CmpLT;				break;
					case TType::RAngle:			op = ArithmeticOp::CmpGT;				break;
					case TType::LessThanEquals:	op = ArithmeticOp::CmpLEq;				break;
					case TType::GreaterEquals:	op = ArithmeticOp::CmpGEq;				break;
					case TType::EqualsTo:		op = ArithmeticOp::CmpEq;				break;
					case TType::NotEquals:		op = ArithmeticOp::CmpNEq;				break;

					case TType::Ampersand:		op = ArithmeticOp::BitwiseAnd;			break;
					case TType::Pipe:			op = ArithmeticOp::BitwiseOr;			break;
					case TType::LogicalOr:		op = ArithmeticOp::LogicalOr;			break;
					case TType::LogicalAnd:		op = ArithmeticOp::LogicalAnd;			break;

					case TType::PlusEq:			op = ArithmeticOp::PlusEquals;			break;
					case TType::MinusEq:		op = ArithmeticOp::MinusEquals;			break;
					case TType::MultiplyEq:		op = ArithmeticOp::MultiplyEquals;		break;
					case TType::DivideEq:		op = ArithmeticOp::DivideEquals;		break;
					case TType::ModEq:			op = ArithmeticOp::ModEquals;			break;
					case TType::ShiftLeftEq:	op = ArithmeticOp::ShiftLeftEquals;		break;
					case TType::ShiftRightEq:	op = ArithmeticOp::ShiftRightEquals;	break;
					case TType::Period:			op = ArithmeticOp::MemberAccess;		break;
					case TType::DoubleColon:	op = ArithmeticOp::ScopeResolution;		break;
					case TType::As:				op = (tok_op.text == "as!") ? ArithmeticOp::ForcedCast : ArithmeticOp::Cast;
												break;
					default:
					{
						if(ps.cgi->customOperatorMapRev.find(tok_op.text) != ps.cgi->customOperatorMapRev.end())
						{
							op = ps.cgi->customOperatorMapRev[tok_op.text];
							break;
						}
						else
						{
							parserError("Unknown operator '%s'", tok_op.text.c_str());
						}
					}
				}
			}
















			Expr* rhs = (tok_op.type == TType::As) ? parseType(ps) : parseUnary(ps);
			if(!rhs)
				return nullptr;

			int next = getCurOpPrec(ps);

			if(next > prec || isRightAssociativeOp(ps.front()))
			{
				rhs = parseRhs(ps, rhs, prec + 1);
				if(!rhs)
					return nullptr;
			}

			ps.currentOpPrec = prec;





			if(op == ArithmeticOp::MemberAccess)
				lhs = CreateAST(MemberAccess, tok_op, lhs, rhs);

			else
				lhs = CreateAST(BinOp, tok_op, lhs, op, rhs);
		}
	}

	Expr* parseIdExpr(ParserState& ps)
	{
		Token tok_id = ps.eat();
		std::string id = tok_id.text;
		VarRef* idvr = CreateAST(VarRef, tok_id, id);

		if(ps.front().type == TType::LParen)
		{
			delete idvr;
			return parseFuncCall(ps, id);
		}
		else
		{
			return idvr;
		}
	}

	Alloc* parseAlloc(ParserState& ps)
	{
		Token tok_alloc = ps.eat();
		iceAssert(tok_alloc.type == TType::Alloc);

		// todo: alloc multidimensional arrays
		Alloc* ret = CreateAST(Alloc, tok_alloc);


		// todo:
		// check for comma, to allocate arrays on the heap
		// ie. let arr = alloc [1, 2, 3].
		// obviously, type is not necessary.
		// probably. if we need to (for polymorphism, to specify the base type, for example)
		// then either

		// alloc: Type [1, 2, 3] or alloc [1, 2, 3]: Type will work.
		// not too hard to implement either.



		while(ps.front().type == TType::LSquare)
		{
			ps.eat();

			// this parses multi-dim array types.
			ret->counts.push_back(parseExpr(ps));

			Token n = ps.eat();
			if(n.type != TType::RSquare)
				parserError("Expected ']', have %s", n.text.c_str());
		}


		auto ct = parseType(ps);
		std::string type = ct->type.strType;
		delete ct;

		if(ps.front().type == TType::LParen)
		{
			// alloc[...] Foo(...)
			FuncCall* fc = parseFuncCall(ps, type);
			ret->params = fc->params;
		}

		ret->type = type;

		return ret;
	}

	Dealloc* parseDealloc(ParserState& ps)
	{
		Token tok_dealloc = ps.eat();
		iceAssert(tok_dealloc.type == TType::Dealloc);

		Expr* expr = parseExpr(ps);
		return CreateAST(Dealloc, tok_dealloc, expr);
	}

	Number* parseNumber(ParserState& ps)
	{
		Number* n;
		if(ps.front().type == TType::Integer)
		{
			Token tok = ps.eat();
			n = CreateAST(Number, tok, getIntegerValue(tok));

			// todo: handle integer suffixes
			n->type = "Int64";

			// set the type.
			// always used signed
		}
		else if(ps.front().type == TType::Decimal)
		{
			Token tok = ps.eat();
			n = CreateAST(Number, tok, getDecimalValue(tok));

			if(n->dval < FLT_MAX)	n->type = "Float32";
			else					n->type = "Float64";
		}
		else
		{
			parserError("What!????");
			iceAssert(false);
			return nullptr;
		}

		return n;
	}

	FuncCall* parseFuncCall(ParserState& ps, std::string id)
	{
		Token tk = ps.eat();
		iceAssert(tk.type == TType::LParen);

		std::deque<Expr*> args;

		size_t paramlen = 0;
		if(ps.front().type != TType::RParen)
		{
			while(true)
			{
				Expr* arg = parseExpr(ps);

				if(arg == nullptr)
					return nullptr;

				paramlen += arg->pin.len;

				args.push_back(arg);
				if(ps.front().type == TType::RParen)
				{
					ps.eat();
					break;
				}

				Token t;
				if((t = ps.eat()).type != TType::Comma)
					parserError("Expected either ',' or ')' in parameter list, got '%s' (id = %s)", t.text.c_str(), id.c_str());
			}
		}
		else
		{
			ps.eat();
		}

		auto ret = CreateASTPos(FuncCall, tk.pin.file, tk.pin.line, tk.pin.col - id.length() + 3, id.length() + 1 + paramlen, id, args);

		return ret;
	}

	Return* parseReturn(ParserState& ps)
	{
		Token front = ps.eat();
		iceAssert(front.type == TType::Return);

		Expr* retval = nullptr;

		// kinda hack: if the next token is a closing brace, then we don't expect an expression
		// this works most of the time.
		if(ps.front().type != TType::RBrace)
			retval = parseExpr(ps);

		return CreateAST(Return, front, retval);
	}

	Expr* parseIf(ParserState& ps)
	{
		Token tok_if = ps.eat();
		iceAssert(tok_if.type == TType::If);

		typedef std::pair<Expr*, BracedBlock*> CCPair;
		std::deque<CCPair> conds;

		Expr* cond = parseExpr(ps);
		BracedBlock* tcase = parseBracedBlock(ps);

		conds.push_back(CCPair(cond, tcase));

		// check for else and else if
		BracedBlock* ecase = nullptr;
		bool parsedElse = false;
		while(ps.front().type == TType::Else)
		{
			ps.eat();
			if(ps.front().type == TType::If && !parsedElse)
			{
				ps.eat();

				// parse an expr, then a block
				Expr* c = parseExpr(ps);
				BracedBlock* cl = parseBracedBlock(ps);

				conds.push_back(CCPair(c, cl));
			}
			else if(!parsedElse)
			{
				parsedElse = true;
				ecase = parseBracedBlock(ps);
			}
			else
			{
				if(parsedElse && ps.front().type != TType::If)
				{
					parserError("Duplicate 'else' clause, only one else clause is permitted per if.");
				}
				else
				{
					parserError("The 'else' clause must be the last block in the if statement.");
				}
			}
		}

		return CreateAST(IfStmt, tok_if, conds, ecase);
	}

	WhileLoop* parseWhile(ParserState& ps)
	{
		Token tok_while = ps.eat();

		if(tok_while.type == TType::While)
		{
			Expr* cond = parseExpr(ps);
			BracedBlock* body = parseBracedBlock(ps);

			return CreateAST(WhileLoop, tok_while, cond, body, false);
		}
		else
		{
			iceAssert(tok_while.type == TType::Do || tok_while.type == TType::Loop);

			// parse the block first
			BracedBlock* body = parseBracedBlock(ps);

			// syntax treat: since a raw block is ignored (for good reason, how can we reference it?)
			// we can use 'do' to run an anonymous block
			// therefore, the 'while' clause at the end is optional; if it's not present, then the condition is false.

			// 'loop' and 'do', when used without the 'while' clause on the end, have opposite behaviours
			// do { ... } runs the block only once, while loop { ... } runs it infinite times.
			// with the 'while' clause, they have the same behaviour.

			Expr* cond = 0;
			if(ps.front().type == TType::While)
			{
				ps.eat();
				cond = parseExpr(ps);
			}
			else
			{
				// here's the magic: continue condition is 'false' for do, 'true' for loop
				cond = CreateAST(BoolVal, ps.front(), tok_while.type == TType::Do ? false : true);
			}

			return CreateAST(WhileLoop, tok_while, cond, body, true);
		}
	}

	ForLoop* parseFor(ParserState& ps)
	{
		Token tok_for = ps.eat();
		iceAssert(tok_for.type == TType::For);

		return 0;
	}


	static void parseInheritanceList(ParserState& ps, Class* cls)
	{
		while(true)
		{
			Token id = ps.eat();
			if(id.type != TType::Identifier)
				parserError("Expected identifier after ':' in struct or class declaration");

			if(std::find(cls->protocolstrs.begin(), cls->protocolstrs.end(), id.text) != cls->protocolstrs.end())
				parserError("Duplicate member %s in inheritance list", id.text.c_str());

			if(cls->name == id.text)
				parserError("Self inheritance is illegal");

			cls->protocolstrs.push_back(id.text);
			ps.skipNewline();

			if(ps.front().type != TType::Comma)
				break;

			ps.eat();
		}
	}

	static void parseGenericTypeList(ParserState& ps, StructBase* sb)
	{
		while(true)
		{
			Token type = ps.eat();
			if(std::find(sb->genericTypes.begin(), sb->genericTypes.end(), type.text) != sb->genericTypes.end())
				parserError("Duplicate generic type %s", type.text.c_str());

			sb->genericTypes.push_back(type.text);

			if(ps.front().type == TType::Comma)
				ps.eat();

			else if(ps.front().type == TType::RAngle)
				break;

			else
				parserError("Unexpected token %s in generic type list", ps.front().text.c_str());
		}

		iceAssert(ps.eat().type == TType::RAngle);
	}






	Struct* parseStruct(ParserState& ps)
	{
		Token tok_str = ps.eat();
		iceAssert(tok_str.type == TType::Struct);

		ps.isParsingStruct = true;
		Token tok_id = ps.eat();

		if(tok_id.type != TType::Identifier)
			parserError("Expected identifier");

		std::string id = tok_id.text;
		Struct* str = CreateAST(Struct, tok_id, id);

		uint64_t attr = checkAndApplyAttributes(ps, Attr_PackedStruct | Attr_VisPublic | Attr_VisInternal | Attr_VisPrivate);
		if(attr & Attr_PackedStruct)
			str->packed = true;

		str->attribs = attr;

		// check for a colon.
		ps.skipNewline();
		if(ps.front().type == TType::LAngle)
		{
			ps.eat();
			parseGenericTypeList(ps, str);
		}
		if(ps.front().type == TType::Colon)
		{
			parserError("Structs cannot inherit from anything");
		}





		// parse a block.
		BracedBlock* body = parseBracedBlock(ps);
		int i = 0;
		for(Expr* stmt : body->statements)
		{
			if(VarDecl* var = dynamic_cast<VarDecl*>(stmt))
			{
				if(str->nameMap.find(var->name) != str->nameMap.end())
					parserError("Duplicate member '%s'", var->name.c_str());

				str->members.push_back(var);

				// don't take up space in the struct if it's static.
				if(!var->isStatic)
				{
					str->nameMap[var->name] = i;
					i++;
				}
				else
				{
				}
			}
			else if(OpOverload* oo = dynamic_cast<OpOverload*>(stmt))
			{
				str->opOverloads.push_back(oo);
			}
			else if(Func* fn = dynamic_cast<Func*>(stmt))
			{
				parserError(fn->pin, "structs cannot contain functions");
			}
			else
			{
				parserError("Found invalid expression type %s", typeid(*stmt).name());
			}
		}

		ps.isParsingStruct = false;
		delete body;
		return str;
	}





	Class* parseClass(ParserState& ps)
	{
		Token tok_cls = ps.eat();
		iceAssert(tok_cls.type == TType::Class || tok_cls.type == TType::Extension);

		ps.isParsingStruct = true;
		Token tok_id = ps.eat();

		if(tok_id.type != TType::Identifier)
			parserError("Expected identifier (got %s)", tok_id.text.c_str());

		std::string id = tok_id.text;
		Class* cls = CreateAST(Class, tok_id, id);

		uint64_t attr = checkAndApplyAttributes(ps, Attr_VisPublic | Attr_VisInternal | Attr_VisPrivate);
		cls->attribs = attr;

		// check for a colon.
		ps.skipNewline();
		if(ps.front().type == TType::LAngle)
		{
			ps.eat();
			parseGenericTypeList(ps, cls);
		}
		if(ps.front().type == TType::Colon)
		{
			ps.eat();
			parseInheritanceList(ps, cls);
		}






		// parse a block.
		BracedBlock* body = parseBracedBlock(ps);
		int i = 0;
		for(Expr* stmt : body->statements)
		{
			if(ComputedProperty* cprop = dynamic_cast<ComputedProperty*>(stmt))
			{
				for(ComputedProperty* c : cls->cprops)
				{
					if(c->name == cprop->name)
						parserError("Duplicate member '%s'", cprop->name.c_str());
				}

				cls->cprops.push_back(cprop);
			}
			else if(VarDecl* var = dynamic_cast<VarDecl*>(stmt))
			{
				if(cls->nameMap.find(var->name) != cls->nameMap.end())
					parserError("Duplicate member '%s'", var->name.c_str());

				cls->members.push_back(var);

				// don't take up space in the struct if it's static.
				if(!var->isStatic)
				{
					cls->nameMap[var->name] = i;
					i++;
				}
			}
			else if(Func* func = dynamic_cast<Func*>(stmt))
			{
				cls->funcs.push_back(func);
			}
			else if(OpOverload* oo = dynamic_cast<OpOverload*>(stmt))
			{
				cls->opOverloads.push_back(oo);

				cls->funcs.push_back(oo->func);
			}
			else if(StructBase* sb = dynamic_cast<StructBase*>(stmt))
			{
				if(Class* nested = dynamic_cast<Class*>(sb))
					cls->nestedTypes.push_back({ nested, 0 });

				else
					parserError("Only class definitions can be nested within other types");
			}
			else if(dynamic_cast<DummyExpr*>(stmt))
			{
				continue;
			}
			else
			{
				parserError("Found invalid expression type %s", typeid(*stmt).name());
			}
		}

		ps.isParsingStruct = false;
		delete body;
		return cls;
	}

	Extension* parseExtension(ParserState& ps)
	{
		Token tok_ext = ps.eat();
		iceAssert(tok_ext.type == TType::Extension);

		Extension* ext = CreateAST(Extension, tok_ext, "");
		Class* cls = parseClass(ps);

		ext->attribs		= cls->attribs;
		ext->funcs			= cls->funcs;
		ext->opOverloads	= cls->opOverloads;
		ext->members		= cls->members;
		ext->nameMap		= cls->nameMap;
		ext->name			= cls->name;
		ext->cprops			= cls->cprops;
		ext->protocolstrs	= cls->protocolstrs;

		delete cls;
		return ext;
	}







	Ast::Enumeration* parseEnum(ParserState& ps)
	{
		iceAssert(ps.eat().type == TType::Enum);

		Token tok_id;
		if((tok_id = ps.eat()).type != TType::Identifier)
			parserError("Expected identifier after 'enum'");

		if(ps.eat().type != TType::LBrace)
			parserError("Expected body after 'enum'");


		if(ps.front().type == TType::RBrace)
			parserError("Empty enumerations are not allowed");


		Enumeration* enumer = CreateAST(Enumeration, tok_id, tok_id.text);
		Token front = ps.front();

		uint64_t attr = checkAndApplyAttributes(ps, Attr_StrongTypeAlias | Attr_VisPublic | Attr_VisInternal | Attr_VisPrivate);
		if(attr & Attr_StrongTypeAlias)
			enumer->isStrong = true;

		// parse the stuff.
		bool isFirst = true;
		bool isNumeric = false;
		Number* prevNumber = nullptr;

		while(front = ps.front(), ps.tokens.size() > 0)
		{
			if(front.type == TType::RBrace && !isFirst)
				break;

			if(front.type != TType::Case)
				parserError("Only 'case' expressions are allowed inside enumerations, got '%s'", front.text.c_str());

			ps.eat();
			if((front = ps.eat()).type != TType::Identifier)
				parserError("Expected identifier after 'case', got '%s'", front.text.c_str());

			std::string eName = front.text;
			Expr* value = 0;

			ps.skipNewline();
			front = ps.front();
			if(front.type == TType::Equal)
			{
				ps.eat();
				value = parseExpr(ps);

				if((prevNumber = dynamic_cast<Number*>(value)))
					isNumeric = true;
			}
			else
			{
				if(isNumeric)
				{
					int64_t val = 0;
					if(prevNumber)
						val = prevNumber->ival + 1;

					// increment it.
					prevNumber = CreateAST(Number, front, val);

					value = prevNumber;
				}
				else if(isFirst)
				{
					int64_t val = 0;
					prevNumber = CreateAST(Number, front, val);

					isNumeric = true;
					value = prevNumber;
				}
				else
				{
					parserError("Enum case '%s' has no explicit value, and value cannot be inferred from previous cases", eName.c_str());
				}
			}


			ps.skipNewline();

			front = ps.front();

			iceAssert(value);
			enumer->cases.push_back(std::make_pair(eName, value));

			isFirst = false;

			if(front.type == TType::Case)
			{
				// ...
			}
			else if(front.type != TType::RBrace)
			{
				parserError("Unexpected token '%s'", front.text.c_str());
			}
			else
			{
				ps.eat();
				break;
			}

		}

		return enumer;
	}

	void parseAttribute(ParserState& ps)
	{
		iceAssert(ps.eat().type == TType::At);
		Token id = ps.eat();

		if(id.type != TType::Identifier && id.type != TType::Private && id.type != TType::Internal && id.type != TType::Public)
			parserError("Expected attribute name after '@'");

		uint64_t attr = 0;
		if(id.text == ATTR_STR_NOMANGLE)			attr |= Attr_NoMangle;
		else if(id.text == ATTR_STR_FORCEMANGLE)	attr |= Attr_ForceMangle;
		else if(id.text == ATTR_STR_NOAUTOINIT)		attr |= Attr_NoAutoInit;
		else if(id.text == ATTR_STR_PACKEDSTRUCT)	attr |= Attr_PackedStruct;
		else if(id.text == ATTR_STR_STRONG)			attr |= Attr_StrongTypeAlias;
		else if(id.text == ATTR_STR_RAW)			attr |= Attr_RawString;
		else if(id.text == "public")
		{
			parserWarn("Attribute 'public' is a keyword, usage as an attribute is deprecated");
			attr |= Attr_VisPublic;
		}
		else if(id.text == "internal")
		{
			parserWarn("Attribute 'internal' is a keyword, usage as an attribute is deprecated");
			attr |= Attr_VisInternal;
		}
		else if(id.text == "private")
		{
			parserWarn("Attribute 'private' is a keyword, usage as an attribute is deprecated");
			attr |= Attr_VisPrivate;
		}
		else if(id.text == "operator")
		{
			// all handled.
			iceAssert(ps.eat().type == TType::LSquare);
			// iceAssert(ps.eat().type == TType::Integer);

			if(ps.front().type == TType::Integer)
			{
				ps.pop_front();
				if(ps.front().type == TType::Comma)
				{
					ps.eat();
					iceAssert(ps.front().type == TType::Identifier);

					if(ps.front().text == "Commutative")
						ps.curAttrib |= Attr_CommutativeOp;

					else if(ps.front().text == "NotCommutative")
						ps.curAttrib &= ~Attr_CommutativeOp;

					else
						parserError("Expected either 'Commutative' or 'NotCommutative' in @operator, got '%s'", ps.front().text.c_str());

					ps.pop_front();
				}
				else if(ps.front().type == TType::RSquare)
				{
					// do nothing
				}
			}
			else if(ps.front().type == TType::Identifier)
			{
				if(ps.front().text == "Commutative")
					ps.curAttrib |= Attr_CommutativeOp;

				else if(ps.front().text == "NotCommutative")
					ps.curAttrib &= ~Attr_CommutativeOp;

				else
					parserError("Expected either 'Commutative' or 'NotCommutative' in @operator, got '%s'", ps.front().text.c_str());

				ps.pop_front();



				if(ps.front().type == TType::Comma)
				{
					ps.eat();
					if(ps.front().type != TType::Integer)
						parserError("Expected integer precedence in @operator");

					ps.eat();
				}
			}

			iceAssert(ps.eat().type == TType::RSquare);
		}
		else										parserError("Unknown attribute '%s'", id.text.c_str());

		ps.curAttrib |= attr;
	}

	Break* parseBreak(ParserState& ps)
	{
		Token tok_br = ps.eat();
		iceAssert(tok_br.type == TType::Break);

		Break* br = CreateAST(Break, tok_br);
		return br;
	}

	Continue* parseContinue(ParserState& ps)
	{
		Token tok_cn = ps.eat();
		iceAssert(tok_cn.type == TType::Continue);

		Continue* cn = CreateAST(Continue, tok_cn);
		return cn;
	}

	Import* parseImport(ParserState& ps)
	{
		iceAssert(ps.eat().type == TType::Import);

		std::string s;
		Token tok_mod = ps.front();
		if(tok_mod.type != TType::Identifier)
			parserError("Expected identifier after import");

		Token t = tok_mod;
		ps.pop_front();

		while(ps.tokens.size() > 0)
		{
			if(t.type == TType::Period)
			{
				s += ".";
			}
			else if(t.type == TType::Identifier)
			{
				s += t.text;
			}
			else if(t.type == TType::Asterisk)
			{
				s += "*";
			}
			else
			{
				break;
			}

			// whitespace handling fucks us up
			t = ps.front();
			ps.pop_front();
		}

		// NOTE: make sure printAst doesn't touch 'cgi', because this will break to hell.
		return CreateAST(Import, tok_mod, s);
	}

	StringLiteral* parseStringLiteral(ParserState& ps)
	{
		iceAssert(ps.front().type == TType::StringLiteral);
		Token str = ps.eat();


		// reference hack in tokeniser.cpp
		str.text = str.text.substr(1);
		auto ret = CreateAST(StringLiteral, str, str.text);

		uint64_t attr = checkAndApplyAttributes(ps, Attr_RawString);
		if(attr & Attr_RawString)
			ret->isRaw = true;

		return ret;
	}

	TypeAlias* parseTypeAlias(ParserState& ps)
	{
		iceAssert(ps.eat().type == TType::TypeAlias);
		Token tok_name = ps.eat();
		if(tok_name.type != TType::Identifier)
			parserError("Expected identifier after 'typealias'");

		if(ps.eat().type != TType::Equal)
			parserError("Expected '='");


		auto ret = CreateAST(TypeAlias, tok_name, tok_name.text, "");

		Expr* ct = parseType(ps);
		iceAssert(ct);

		ret->origType = ct->type.strType;
		delete ct;

		uint64_t attr = checkAndApplyAttributes(ps, Attr_StrongTypeAlias);
		if(attr & Attr_StrongTypeAlias)
			ret->isStrong = true;

		return ret;
	}

	DeferredExpr* parseDefer(ParserState& ps)
	{
		iceAssert(ps.front().type == TType::Defer);
		return CreateAST(DeferredExpr, ps.eat(), parseExpr(ps));
	}

	Typeof* parseTypeof(ParserState& ps)
	{
		iceAssert(ps.front().type == TType::Typeof);
		return CreateAST(Typeof, ps.eat(), parseExpr(ps));
	}

	ArrayLiteral* parseArrayLiteral(ParserState& ps)
	{
		iceAssert(ps.front().type == TType::LSquare);
		Token front = ps.eat();

		std::deque<Expr*> values;
		while(true)
		{
			Token tok = ps.front();
			if(tok.type == TType::Comma)
			{
				ps.eat();
				continue;
			}
			else if(tok.type == TType::RSquare)
			{
				break;
			}
			else
			{
				values.push_back(parseExpr(ps));
			}
		}

		iceAssert(ps.front().type == TType::RSquare);
		ps.eat();

		return CreateAST(ArrayLiteral, front, values);
	}

	NamespaceDecl* parseNamespace(ParserState& ps)
	{
		iceAssert(ps.eat().type == TType::Namespace);
		Token tok_id = ps.eat();

		// todo: handle "namespace Foo.Bar.Baz { }", which c++ technically still doesn't have
		// (still a c++1z thing)
		if(tok_id.type != TType::Identifier)
			parserError("Expected identifier after namespace declaration");

		BracedBlock* bb = parseBracedBlock(ps);
		NamespaceDecl* ns = CreateAST(NamespaceDecl, tok_id, tok_id.text, bb);

		return ns;
	}



	ArithmeticOp mangledStringToOperator(Codegen::CodegenInstance* cgi, std::string op)
	{
		if(op == "aS")		return ArithmeticOp::Assign;
		else if(op == "pL") return ArithmeticOp::PlusEquals;
		else if(op == "mI") return ArithmeticOp::MinusEquals;
		else if(op == "mL") return ArithmeticOp::MultiplyEquals;
		else if(op == "dV") return ArithmeticOp::DivideEquals;
		else if(op == "rM") return ArithmeticOp::ModEquals;
		else if(op == "aN") return ArithmeticOp::BitwiseAndEquals;
		else if(op == "oR") return ArithmeticOp::BitwiseOrEquals;
		else if(op == "eO") return ArithmeticOp::BitwiseXorEquals;
		else if(op == "lS") return ArithmeticOp::ShiftLeftEquals;
		else if(op == "rS") return ArithmeticOp::ShiftRightEquals;

		else if(op == "ad") return ArithmeticOp::AddrOf;
		else if(op == "de") return ArithmeticOp::Deref;
		else if(op == "nt") return ArithmeticOp::LogicalNot;
		else if(op == "aa") return ArithmeticOp::LogicalAnd;
		else if(op == "oo") return ArithmeticOp::LogicalOr;
		else if(op == "pl") return ArithmeticOp::Add;
		else if(op == "mi") return ArithmeticOp::Subtract;
		else if(op == "ml") return ArithmeticOp::Multiply;
		else if(op == "dv") return ArithmeticOp::Divide;
		else if(op == "rm") return ArithmeticOp::Modulo;
		else if(op == "an") return ArithmeticOp::BitwiseAnd;
		else if(op == "or") return ArithmeticOp::BitwiseOr;
		else if(op == "eo") return ArithmeticOp::BitwiseXor;
		else if(op == "co") return ArithmeticOp::BitwiseNot;
		else if(op == "ls") return ArithmeticOp::ShiftLeft;
		else if(op == "rs") return ArithmeticOp::ShiftRight;
		else if(op == "eq") return ArithmeticOp::CmpEq;
		else if(op == "ne") return ArithmeticOp::CmpNEq;
		else if(op == "lt") return ArithmeticOp::CmpLT;
		else if(op == "gt") return ArithmeticOp::CmpGT;
		else if(op == "le") return ArithmeticOp::CmpLEq;
		else if(op == "ge") return ArithmeticOp::CmpGEq;
		else
		{
			if(cgi->customOperatorMapRev.find(op) != cgi->customOperatorMapRev.end())
				return cgi->customOperatorMapRev[op];

			parserError("Invalid operator '%s'", op.c_str());
		}
	}

	std::string operatorToMangledString(Codegen::CodegenInstance* cgi, ArithmeticOp op)
	{
		// see https://refspecs.linuxbase.org/cxxabi-1.75.html#mangling-operator
		switch(op)
		{
			case ArithmeticOp::Assign:				return "aS";
			case ArithmeticOp::PlusEquals:			return "pL";
			case ArithmeticOp::MinusEquals:			return "mI";
			case ArithmeticOp::MultiplyEquals:		return "mL";
			case ArithmeticOp::DivideEquals:		return "dV";
			case ArithmeticOp::ModEquals:			return "rM";
			case ArithmeticOp::BitwiseAndEquals:	return "aN";
			case ArithmeticOp::BitwiseOrEquals:		return "oR";
			case ArithmeticOp::BitwiseXorEquals:	return "eO";
			case ArithmeticOp::ShiftLeftEquals:		return "lS";
			case ArithmeticOp::ShiftRightEquals:	return "rS";
			case ArithmeticOp::AddrOf:				return "ad";
			case ArithmeticOp::Deref:				return "de";
			case ArithmeticOp::LogicalNot:			return "nt";
			case ArithmeticOp::LogicalAnd:			return "aa";
			case ArithmeticOp::LogicalOr:			return "oo";
			case ArithmeticOp::Add:					return "pl";
			case ArithmeticOp::Subtract:			return "mi";
			case ArithmeticOp::Multiply:			return "ml";
			case ArithmeticOp::Divide:				return "dv";
			case ArithmeticOp::Modulo:				return "rm";
			case ArithmeticOp::BitwiseAnd:			return "an";
			case ArithmeticOp::BitwiseOr:			return "or";
			case ArithmeticOp::BitwiseXor:			return "eo";
			case ArithmeticOp::BitwiseNot:			return "co";
			case ArithmeticOp::ShiftLeft:			return "ls";
			case ArithmeticOp::ShiftRight:			return "rs";
			case ArithmeticOp::CmpEq:				return "eq";
			case ArithmeticOp::CmpNEq:				return "ne";
			case ArithmeticOp::CmpLT:				return "lt";
			case ArithmeticOp::CmpGT:				return "gt";
			case ArithmeticOp::CmpLEq:				return "le";
			case ArithmeticOp::CmpGEq:				return "ge";
			default:								return cgi->customOperatorMap[op].first;
		}
	}

	OpOverload* parseOpOverload(ParserState& ps)
	{
		// if(!ps.isParsingStruct)
		// 	parserError("Can only overload operators in the context of a named aggregate type");

		iceAssert(ps.eat().text == "operator");
		Token op = ps.eat();

		ArithmeticOp ao;

		if(op.type == TType::Equal)				ao = ArithmeticOp::Assign;
		else if(op.type == TType::EqualsTo)		ao = ArithmeticOp::CmpEq;
		else if(op.type == TType::NotEquals)	ao = ArithmeticOp::CmpNEq;
		else if(op.type == TType::Plus)			ao = ArithmeticOp::Add;
		else if(op.type == TType::Minus)		ao = ArithmeticOp::Subtract;
		else if(op.type == TType::Asterisk)		ao = ArithmeticOp::Multiply;
		else if(op.type == TType::Divide)		ao = ArithmeticOp::Divide;

		else if(op.type == TType::PlusEq)		ao = ArithmeticOp::PlusEquals;
		else if(op.type == TType::MinusEq)		ao = ArithmeticOp::MinusEquals;
		else if(op.type == TType::MultiplyEq)	ao = ArithmeticOp::MultiplyEquals;
		else if(op.type == TType::DivideEq)		ao = ArithmeticOp::DivideEquals;
		else
		{
			if(ps.cgi->customOperatorMapRev.find(op.text) != ps.cgi->customOperatorMapRev.end())
				ao = ps.cgi->customOperatorMapRev[op.text];

			else
				parserError("Unsupported operator overload on operator '%s'", op.text.c_str());
		}


		OpOverload* oo = CreateAST(OpOverload, op, ao);

		Token fake;
		fake.pin = ps.currentPos;
		fake.text = "operator#" + operatorToMangledString(ps.cgi, ao);
		fake.type = TType::Identifier;

		ps.tokens.push_front(fake);

		// parse a func declaration.
		uint64_t attr = checkAndApplyAttributes(ps, Attr_VisPublic | Attr_VisInternal | Attr_VisPrivate | Attr_CommutativeOp);
		oo->func = parseFunc(ps);
		oo->func->decl->attribs = attr & ~Attr_CommutativeOp;

		oo->attribs = attr;

		// check number of arguments
		// note: this is without the "self" parameter, so args == 1 --> binop
		// args == 0 --> unary op.

		// if this is not in a struct, then 2 == binop, 1 == unaryop.

		if(attr & Attr_CommutativeOp)
		{
			oo->isCommutative = true;
		}

		if(ps.isParsingStruct)
		{
			oo->isInType = true;
			if(oo->func->decl->params.size() == 1)
			{
				oo->isBinOp = true;
			}
			else if(oo->func->decl->params.size() == 0)
			{
				oo->isBinOp = false;
			}
			else
			{
				parserError(ps, "Invalid number of parameters to operator overload; expected 0 or 1, got %zu",
					oo->func->decl->params.size());
			}
		}
		else
		{
			oo->isInType = false;
			if(oo->func->decl->params.size() == 2)
			{
				oo->isBinOp = true;
			}
			else if(oo->func->decl->params.size() == 1)
			{
				oo->isBinOp = false;
			}
			else
			{
				parserError(ps, "Invalid number of parameters to operator overload; expected 1 or 2, got %zu",
					oo->func->decl->params.size());
			}
		}

		return oo;
	}
























	// come on man
	void parserError(const char* msg, ...) __attribute__((format(printf, 1, 2)));
	void parserError(const char* msg, ...)
	{
		va_list ap;
		va_start(ap, msg);

		__error_gen(staticState->curtok.pin.line, staticState->curtok.pin.col, staticState->curtok.pin.len, staticState->curtok.pin.file,
			msg, "Error", true, ap);

		va_end(ap);
		abort();
	}

	// grr
	void parserWarn(const char* msg, ...) __attribute__((format(printf, 1, 2)));
	void parserWarn(const char* msg, ...)
	{
		va_list ap;
		va_start(ap, msg);

		__error_gen(staticState->curtok.pin.line, staticState->curtok.pin.col, staticState->curtok.pin.len, staticState->curtok.pin.file,
			msg, "Warning", false, ap);

		va_end(ap);
	}


	void parserError(ParserState& ps, const char* msg, ...) __attribute__((format(printf, 2, 3)));
	void parserError(ParserState& ps, const char* msg, ...)
	{
		va_list ap;
		va_start(ap, msg);

		__error_gen(ps.curtok.pin.line, ps.curtok.pin.col, ps.curtok.pin.len, ps.curtok.pin.file, msg, "Error", true, ap);

		va_end(ap);
		abort();
	}

	void parserWarn(ParserState& ps, const char* msg, ...) __attribute__((format(printf, 2, 3)));
	void parserWarn(ParserState& ps, const char* msg, ...)
	{
		va_list ap;
		va_start(ap, msg);

		__error_gen(ps.curtok.pin.line, ps.curtok.pin.col, ps.curtok.pin.len, ps.curtok.pin.file, msg, "Warning", false, ap);

		va_end(ap);
	}





	void parserError(Token tok, const char* msg, ...) __attribute__((format(printf, 2, 3)));
	void parserError(Token tok, const char* msg, ...)
	{
		va_list ap;
		va_start(ap, msg);

		__error_gen(tok.pin.line, tok.pin.col, tok.pin.len, tok.pin.file, msg, "Error", true, ap);

		va_end(ap);
		abort();
	}

	void parserWarn(Token tok, const char* msg, ...) __attribute__((format(printf, 2, 3)));
	void parserWarn(Token tok, const char* msg, ...)
	{
		va_list ap;
		va_start(ap, msg);

		__error_gen(tok.pin.line, tok.pin.col, tok.pin.len, tok.pin.file, msg, "Warning", false, ap);

		va_end(ap);
	}




	void parserError(Pin pin, const char* msg, ...) __attribute__((format(printf, 2, 3)));
	void parserError(Pin pin, const char* msg, ...)
	{
		va_list ap;
		va_start(ap, msg);

		__error_gen(pin.line, pin.col, pin.len, pin.file, msg, "Error", true, ap);

		va_end(ap);
		abort();
	}

	void parserWarn(Pin pin, const char* msg, ...) __attribute__((format(printf, 2, 3)));
	void parserWarn(Pin pin, const char* msg, ...)
	{
		va_list ap;
		va_start(ap, msg);

		__error_gen(pin.line, pin.col, pin.len, pin.file, msg, "Warning", false, ap);

		va_end(ap);
	}





	void parserError(ParserState& ps, Token tok, const char* msg, ...) __attribute__((format(printf, 3, 4)));
	void parserError(ParserState& ps, Token tok, const char* msg, ...)
	{
		va_list ap;
		va_start(ap, msg);

		__error_gen(tok.pin.line, tok.pin.col, tok.pin.len, tok.pin.file, msg, "Error", true, ap);

		va_end(ap);
		abort();
	}

	void parserWarn(ParserState& ps, Token tok, const char* msg, ...) __attribute__((format(printf, 3, 4)));
	void parserWarn(ParserState& ps, Token tok, const char* msg, ...)
	{
		va_list ap;
		va_start(ap, msg);

		__error_gen(tok.pin.line, tok.pin.col, tok.pin.len, tok.pin.file, msg, "Warning", false, ap);

		va_end(ap);
	}
}




namespace Ast
{
	uint64_t Attr_Invalid			= 0x00;
	uint64_t Attr_NoMangle			= 0x01;
	uint64_t Attr_VisPublic			= 0x02;
	uint64_t Attr_VisInternal		= 0x04;
	uint64_t Attr_VisPrivate		= 0x08;
	uint64_t Attr_ForceMangle		= 0x10;
	uint64_t Attr_NoAutoInit		= 0x20;
	uint64_t Attr_PackedStruct		= 0x40;
	uint64_t Attr_StrongTypeAlias	= 0x80;
	uint64_t Attr_RawString			= 0x100;
	uint64_t Attr_Override			= 0x200;
	uint64_t Attr_CommutativeOp		= 0x400;
}