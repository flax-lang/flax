// Parser.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include <map>
#include <deque>
#include <cfloat>
#include <fstream>
#include <cassert>
#include <cinttypes>
#include "../include/ast.h"
#include "../include/parser.h"
#include "../include/compiler.h"
#include "../include/codegen.h"

using namespace Ast;


namespace Parser
{
	static Token curtok;
	static PosInfo currentPos;
	static Root* rootNode						= nullptr;
	static uint32_t curAttrib					= 0;

	// todo: hacks
	static bool isParsingStruct;
	static bool didHaveLeftParen;
	static int currentOperatorPrecedence;

	#define CreateAST_Raw(name, ...)		(new name (currentPos, ##__VA_ARGS__))
	#define CreateAST(name, tok, ...)		(new name (tok.posinfo, ##__VA_ARGS__))


	#define ATTR_STR_NOMANGLE			"nomangle"
	#define ATTR_STR_FORCEMANGLE		"forcemangle"
	#define ATTR_STR_NOAUTOINIT			"noinit"
	#define ATTR_STR_PACKEDSTRUCT		"packed"
	#define ATTR_STR_STRONG				"strong"
	#define ATTR_STR_RAW				"raw"



	static void parserError(Token token, const char* msg, va_list args)
	{
		char* alloc = nullptr;
		vasprintf(&alloc, msg, args);

		fprintf(stderr, "%s(%s:%" PRIu64 ":%" PRIu64 ")%s Error%s: %s\n\n", COLOUR_BLACK_BOLD, token.posinfo.file.c_str(), token.posinfo.line, token.posinfo.col, COLOUR_RED_BOLD, COLOUR_RESET, alloc);
	}

	// come on man
	void parserError(const char* msg, ...) __attribute__((format(printf, 1, 2)));
	void parserError(const char* msg, ...)
	{
		va_list ap;
		va_start(ap, msg);

		parserError(curtok, msg, ap);

		va_end(ap);

		fprintf(stderr, "There were errors, compilation cannot continue\n");
		abort();
	}

	// grr
	void parserWarn(const char* msg, ...) __attribute__((format(printf, 1, 2)));
	void parserWarn(const char* msg, ...)
	{
		if(Compiler::getFlag(Compiler::Flag::NoWarnings))
			return;

		va_list ap;
		va_start(ap, msg);

		char* alloc = nullptr;
		vasprintf(&alloc, msg, ap);

		fprintf(stderr, "%s(%s:%" PRIu64 ":%" PRIu64 ")%s Warning%s: %s\n\n", COLOUR_BLACK_BOLD, curtok.posinfo.file.c_str(), curtok.posinfo.line, curtok.posinfo.col, COLOUR_MAGENTA_BOLD, COLOUR_RESET, alloc);

		va_end(ap);
		free(alloc);

		if(Compiler::getFlag(Compiler::Flag::WarningsAsErrors))
			parserError("Treating warning as error because -Werror was passed");
	}


	std::string getModuleName(std::string filename)
	{
		size_t lastdot = filename.find_last_of(".");
		std::string modname = (lastdot == std::string::npos ? filename : filename.substr(0, lastdot));

		size_t sep = modname.find_last_of("\\/");
		if(sep != std::string::npos)
			modname = modname.substr(sep + 1, modname.length() - sep - 1);

		return modname;
	}

	// helpers
	static void skipNewline(TokenList& tokens)
	{
		// eat newlines AND comments
		while(tokens.size() > 0 && (tokens.front().type == TType::NewLine || tokens.front().type == TType::Comment || tokens.front().type == TType::Semicolon))
		{
			tokens.pop_front();
			currentPos.line++;
		}
	}

	static Token eat(TokenList& tokens)
	{
		// returns the current front, then pops front.
		if(tokens.size() == 0)
			parserError("Unexpected end of input");

		skipNewline(tokens);
		Token t = tokens.front();
		tokens.pop_front();
		skipNewline(tokens);

		curtok = t;
		return t;
	}

	static bool checkHasMore(TokenList& tokens)
	{
		return tokens.size() > 0;
	}

	static bool isRightAssociativeOp(Token tok)
	{
		// if(tok.type == TType::Period)
			// return true;

		return false;
	}

	static bool isPostfixUnaryOperator(TType tt)
	{
		return (tt == TType::LSquare) || (tt == TType::DoublePlus) || (tt == TType::DoubleMinus);
	}

	static int getOpPrec(Token tok)
	{
		switch(tok.type)
		{
			case TType::Comma:
				return didHaveLeftParen ? 2000 : -1;	// lol x3

			case TType::As:
			case TType::Period:
				return 150;

			// array index: 120
			case TType::LSquare:
				return 120;

			case TType::DoublePlus:
			case TType::DoubleMinus:
				return 110;

			case TType::Asterisk:
			case TType::Divide:
			case TType::Percent:
				return 100;

			case TType::Plus:
			case TType::Minus:
				return 90;

			case TType::ShiftLeft:
			case TType::ShiftRight:
				return 80;

			case TType::LAngle:
			case TType::RAngle:
			case TType::LessThanEquals:
			case TType::GreaterEquals:
				return 70;

			case TType::EqualsTo:
			case TType::NotEquals:
				return 60;

			case TType::Ampersand:
				return 50;

			case TType::Pipe:
				return 40;

			case TType::LogicalAnd:
				return 30;

			case TType::LogicalOr:
				return 20;

			case TType::Equal:
			case TType::PlusEq:
			case TType::MinusEq:
			case TType::MultiplyEq:
			case TType::DivideEq:
			case TType::ModEq:
			case TType::ShiftLeftEq:
			case TType::ShiftRightEq:
				return 10;

			default:
				return -1;
		}
	}

	std::string arithmeticOpToString(Ast::ArithmeticOp op)
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
		}
	}

	static int64_t getIntegerValue(Token t)
	{
		iceAssert(t.type == TType::Integer);
		int base = 10;
		if(t.text.find("0x") == 0)
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
	};

	static uint32_t checkAndApplyAttributes(uint32_t allowed)
	{
		uint32_t disallowed = ~allowed;

		if(curAttrib & disallowed)
		{

			int shifts = 0;
			while(((curAttrib & disallowed) & 1) == 0)
				curAttrib >>= 1, disallowed >>= 1, shifts++;

			if(shifts > 0)
				parserError("Invalid attribute '%s' for expression", ReadableAttrNames[shifts + 1]);
		}

		uint32_t ret = curAttrib;
		curAttrib = 0;
		return ret;
	}



	Root* Parse(std::string filename, std::string str, Codegen::CodegenInstance* cgi)
	{
		Token t;
		currentPos.file = filename;
		currentPos.line = 1;
		currentPos.col = 1;
		curAttrib = 0;



		TokenList tokens;

		// split into lines
		{
			std::stringstream ss(str);

			std::string tmp;
			while(std::getline(ss, tmp, '\n'))
				cgi->rawLines.push_back(tmp);
		}


		while((t = getNextToken(str, currentPos)).text.size() > 0)
			tokens.push_back(t);

		rootNode = new Root();
		currentPos.file = filename;
		currentPos.line = 1;
		currentPos.col = 1;


		// todo: hacks
		isParsingStruct = 0;
		didHaveLeftParen = 0;
		currentOperatorPrecedence = 0;



		skipNewline(tokens);
		parseAll(tokens);

		return rootNode;
	}


	// this only handles the topmost level.
	void parseAll(TokenList& tokens)
	{
		if(tokens.size() == 0)
			return;

		Token tok;
		while(tokens.size() > 0 && (tok = tokens.front()).text.length() > 0)
		{
			switch(tok.type)
			{
				case TType::Func:
					rootNode->topLevelExpressions.push_back(parseFunc(tokens));
					break;

				case TType::Import:
					rootNode->topLevelExpressions.push_back(parseImport(tokens));
					break;

				case TType::ForeignFunc:
					rootNode->topLevelExpressions.push_back(parseForeignFunc(tokens));
					break;

				case TType::Struct:
					rootNode->topLevelExpressions.push_back(parseStruct(tokens));
					break;

				case TType::Enum:
					rootNode->topLevelExpressions.push_back(parseEnum(tokens));
					break;

				case TType::Extension:
					rootNode->topLevelExpressions.push_back(parseExtension(tokens));
					break;

				case TType::Var:
				case TType::Val:
					rootNode->topLevelExpressions.push_back(parseVarDecl(tokens));
					break;

				// shit you just skip
				case TType::NewLine:
					currentPos.line++;
					[[clang::fallthrough]];

				case TType::Comment:
				case TType::Semicolon:
					tokens.pop_front();
					break;

				case TType::TypeAlias:
					rootNode->topLevelExpressions.push_back(parseTypeAlias(tokens));
					break;

				case TType::Private:
					eat(tokens);
					curAttrib |= Attr_VisPrivate;
					break;

				case TType::Internal:
					eat(tokens);
					curAttrib |= Attr_VisInternal;
					break;

				case TType::Public:
					eat(tokens);
					curAttrib |= Attr_VisPublic;
					break;

				case TType::At:
					parseAttribute(tokens);
					break;

				default:	// wip: skip shit we don't know/care about for now
					parserError("Unknown token '%s'", tok.text.c_str());
			}
		}
	}

	Expr* parsePrimary(TokenList& tokens)
	{
		if(tokens.size() == 0)
			return nullptr;

		Token tok;
		while((tok = tokens.front()).text.length() > 0)
		{
			switch(tok.type)
			{
				case TType::Var:
				case TType::Val:
					return parseVarDecl(tokens);

				case TType::Func:
					return parseFunc(tokens);

				case TType::ForeignFunc:
					return parseForeignFunc(tokens);

				case TType::LParen:
					return parseParenthesised(tokens);

				case TType::Identifier:
					if(tok.text == "init")
						return parseInitFunc(tokens);

					else if(tok.text == "operator")
						return parseOpOverload(tokens);

					return parseIdExpr(tokens);

				case TType::Static:
					return parseStaticDecl(tokens);

				case TType::Alloc:
					return parseAlloc(tokens);

				case TType::Dealloc:
					return parseDealloc(tokens);

				case TType::Struct:
					return parseStruct(tokens);

				case TType::Enum:
					return parseEnum(tokens);

				case TType::Defer:
					return parseDefer(tokens);

				case TType::Typeof:
					return parseTypeof(tokens);

				case TType::Extension:
					return parseExtension(tokens);

				case TType::At:
					parseAttribute(tokens);
					return parsePrimary(tokens);

				case TType::StringLiteral:
					return parseStringLiteral(tokens);

				case TType::Integer:
				case TType::Decimal:
					return parseNumber(tokens);

				case TType::LSquare:
					return parseArrayLiteral(tokens);

				case TType::Return:
					return parseReturn(tokens);

				case TType::Break:
					return parseBreak(tokens);

				case TType::Continue:
					return parseContinue(tokens);

				case TType::If:
					return parseIf(tokens);

				// since both have the same kind of AST node, parseWhile can handle both
				case TType::Do:
				case TType::While:
				case TType::Loop:
					return parseWhile(tokens);

				case TType::For:
					return parseFor(tokens);

				// shit you just skip
				case TType::NewLine:
					currentPos.line++;
					[[clang::fallthrough]];

				case TType::Comment:
				case TType::Semicolon:
					eat(tokens);
					return CreateAST(DummyExpr, tok);

				case TType::TypeAlias:
					return parseTypeAlias(tokens);

				case TType::True:
					tokens.pop_front();
					return CreateAST(BoolVal, tok, true);

				case TType::False:
					tokens.pop_front();
					return CreateAST(BoolVal, tok, false);

				case TType::Private:
					eat(tokens);
					curAttrib |= Attr_VisPrivate;
					return parsePrimary(tokens);

				case TType::Internal:
					eat(tokens);
					curAttrib |= Attr_VisInternal;
					return parsePrimary(tokens);

				case TType::Public:
					eat(tokens);
					curAttrib |= Attr_VisPublic;
					return parsePrimary(tokens);

				case TType::LBrace:
					parserWarn("Anonymous blocks are ignored; to run, preface with 'do'");
					parseBracedBlock(tokens);		// parse it, but throw it away
					return CreateAST(DummyExpr, tokens.front());

				default:
					parserError("Unexpected token '%s'\n", tok.text.c_str());
			}
		}

		return nullptr;
	}

	Expr* parseUnary(TokenList& tokens)
	{
		Token front = tokens.front();

		// check for unary shit
		ArithmeticOp op = ArithmeticOp::Invalid;

		if(front.type == TType::Exclamation)		op = ArithmeticOp::LogicalNot;
		else if(front.type == TType::Plus)			op = ArithmeticOp::Plus;
		else if(front.type == TType::Minus)			op = ArithmeticOp::Minus;
		else if(front.type == TType::Tilde)			op = ArithmeticOp::BitwiseNot;
		else if(front.type == TType::Pound)			op = ArithmeticOp::Deref;
		else if(front.type == TType::Ampersand)		op = ArithmeticOp::AddrOf;

		if(op != ArithmeticOp::Invalid)
		{
			eat(tokens);
			return CreateAST(UnaryOp, front, op, parseUnary(tokens));
		}

		return parsePrimary(tokens);
	}

	Expr* parseStaticDecl(TokenList& tokens)
	{
		iceAssert(tokens.front().type == TType::Static);
		if(!isParsingStruct)
			parserError("Static declarations are only allowed inside struct definitions");

		eat(tokens);
		if(tokens.front().type == TType::Func)
		{
			Func* ret = parseFunc(tokens);
			ret->decl->isStatic = true;
			return ret;
		}
		else if(tokens.front().type == TType::Var || tokens.front().type == TType::Val)
		{
			// static var.
			VarDecl* ret = parseVarDecl(tokens);
			ret->isStatic = true;
			return ret;
		}
		else
		{
			parserError("Invaild static expression '%s'", tokens.front().text.c_str());
		}
	}

	FuncDecl* parseFuncDecl(TokenList& tokens)
	{
		// todo: better things? it's right now mostly hacks.
		if(tokens.front().text != "init" && tokens.front().text.find("operator") != 0)
			iceAssert(eat(tokens).type == TType::Func);

		if(tokens.front().type != TType::Identifier)
			parserError("Expected identifier, but got token of type %d", tokens.front().type);

		Token func_id = eat(tokens);
		std::string id = func_id.text;

		std::deque<std::string> genericTypes;

		// expect a left bracket
		Token paren = eat(tokens);
		if(paren.type != TType::LParen && paren.type != TType::LAngle)
		{
			parserError("Expected '(' in function declaration, got '%s'", paren.text.c_str());
		}
		else if(paren.type == TType::LAngle)
		{
			Expr* inner = parseType(tokens);
			iceAssert(inner->type.isLiteral);

			genericTypes.push_back(inner->type.strType);

			Token angleOrComma = eat(tokens);
			if(angleOrComma.type == TType::Comma)
			{
				// parse more.
				Token tok;
				while(true)
				{
					Expr* gtype = parseType(tokens);
					iceAssert(gtype->type.isLiteral);

					genericTypes.push_back(gtype->type.strType);

					tok = eat(tokens);
					if(tok.type == TType::Comma)		continue;
					else if(tok.type == TType::RAngle)	break;
					else								parserError("Expected '>' or ','");
				}
			}
			else if(angleOrComma.type != TType::RAngle)
				parserError("Expected '>' or ','");

			eat(tokens);
		}

		bool isVA = false;
		// get the parameter list
		// expect an identifer, colon, type
		std::deque<VarDecl*> params;
		std::map<std::string, VarDecl*> nameCheck;

		while(tokens.size() > 0 && tokens.front().type != TType::RParen)
		{
			Token tok_id;
			bool immutable = true;
			if((tok_id = eat(tokens)).type != TType::Identifier)
			{
				if(tok_id.type == TType::Elipsis)
				{
					isVA = true;
					if(tokens.front().type != TType::RParen)
						parserError("Vararg declaration must be last in the function declaration");

					break;
				}
				else if(tok_id.type == TType::Var)
				{
					immutable = false;
					tok_id = eat(tokens);
				}
				else if(tok_id.type == TType::Val)
				{
					immutable = false;
					tok_id = eat(tokens);
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
			if(eat(tokens).type != TType::Colon)
				parserError("Expected ':' followed by a type");

			Expr* ctype = parseType(tokens);
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

			if(tokens.front().type == TType::Comma)
				eat(tokens);
		}

		// consume the closing paren
		eat(tokens);

		// get return type.
		std::string ret;
		if(checkHasMore(tokens) && tokens.front().type == TType::Arrow)
		{
			eat(tokens);
			Expr* ctype = parseType(tokens);
			ret = ctype->type.strType;
			delete ctype;

			if(ret == "Void")
				parserWarn("Explicitly specifying 'Void' as the return type is redundant");
		}
		else
		{
			ret = "Void";
		}

		skipNewline(tokens);
		FuncDecl* f = CreateAST(FuncDecl, func_id, id, params, ret);
		f->attribs = checkAndApplyAttributes(Attr_VisPublic | Attr_VisInternal | Attr_VisPrivate | Attr_NoMangle | Attr_ForceMangle);

		f->hasVarArg = isVA;
		f->genericTypes = genericTypes;

		return f;
	}

	ForeignFuncDecl* parseForeignFunc(TokenList& tokens)
	{
		Token func = tokens.front();
		iceAssert(func.type == TType::ForeignFunc);
		eat(tokens);

		FFIType ffitype = FFIType::C;

		// check for specifying the type
		if(tokens.front().type == TType::LParen)
		{
			eat(tokens);
			if(tokens.front().type != TType::Identifier)
				parserError("Expected type of external function, either C (default) or Cpp");

			Token ftype = eat(tokens);
			std::string lftype = ftype.text;
			std::transform(lftype.begin(), lftype.end(), lftype.begin(), ::tolower);

			if(lftype == "c")			ffitype = FFIType::C;
			else if(lftype == "cpp")	ffitype = FFIType::Cpp;
			else						parserError("Unknown FFI type '%s'", ftype.text.c_str());

			if(eat(tokens).type != TType::RParen)
				parserError("Expected ')'");
		}



		FuncDecl* decl = parseFuncDecl(tokens);
		decl->isFFI = true;
		decl->ffiType = ffitype;

		return CreateAST(ForeignFuncDecl, func, decl);
	}

	BracedBlock* parseBracedBlock(TokenList& tokens)
	{
		Token tok_cls = eat(tokens);
		BracedBlock* c = CreateAST(BracedBlock, tok_cls);

		// make sure the first token is a left brace.
		if(tok_cls.type != TType::LBrace)
			parserError("Expected '{' to begin a block, found '%s'!", tok_cls.text.c_str());


		std::deque<DeferredExpr*> defers;

		// get the stuff inside.
		while(tokens.size() > 0 && tokens.front().type != TType::RBrace)
		{
			Expr* e = parseExpr(tokens);
			DeferredExpr* d = nullptr;

			if((d = dynamic_cast<DeferredExpr*>(e)))
			{
				defers.push_front(d);
			}
			else
			{
				c->statements.push_back(e);
			}

			skipNewline(tokens);
		}

		if(eat(tokens).type != TType::RBrace)
			parserError("Expected '}'");



		for(auto d : defers)
		{
			c->deferredStatements.push_back(d);
		}

		return c;
	}

	Func* parseFunc(TokenList& tokens)
	{
		Token front = tokens.front();
		FuncDecl* decl = parseFuncDecl(tokens);

		auto ret = CreateAST(Func, front, decl, parseBracedBlock(tokens));
		rootNode->allFunctionBodies.push_back(ret);

		return ret;
	}


	Expr* parseInitFunc(TokenList& tokens)
	{
		Token front = tokens.front();
		iceAssert(front.text == "init");

		// we need to disambiguate between calling the init() function, and defining an init() function
		// to do this, we can loop through the tokens (without consuming) until we find the closing ')'
		// then see if the token following that is a '{'. if so, it's a declaration, if not it's a call

		if(tokens.size() < 3)
			parserError("Unexpected end of input");

		else if(tokens.size() > 3 && tokens[1].type != TType::LParen)
			parserError("Expected '(' for either function call or declaration");

		int parenLevel = 0;
		bool foundBrace = false;
		for(size_t i = 1; i < tokens.size(); i++)
		{
			if(tokens[i].type == TType::LParen)
			{
				parenLevel++;
			}
			else if(tokens[i].type == TType::RParen)
			{
				parenLevel--;
				if(parenLevel == 0)
				{
					// look through each until we find a brace
					for(size_t k = i + 1; k < tokens.size() && !foundBrace; k++)
					{
						if(tokens[k].type == TType::Comment || tokens[k].type == TType::NewLine)
							continue;

						else if(tokens[k].type == TType::LBrace)
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
			FuncDecl* decl = parseFuncDecl(tokens);
			return CreateAST(Func, front, decl, parseBracedBlock(tokens));
		}
		else
		{
			// no brace, it's a call
			// eat the "init" token
			eat(tokens);
			return parseFuncCall(tokens, "init");
		}
	}


















	Expr* parseType(TokenList& tokens)
	{
		Token tmp = eat(tokens);

		if(tmp.type == TType::Identifier)
		{
			std::string baseType = tmp.text;

			// parse until we get a non-identifier and non-scoperes
			{
				bool expectingScope = true;
				Token t = tokens.front();
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

					eat(tokens);
					t = tokens.front();
				}
			}

			std::string ptrAppend = "";
			if(tokens.size() > 0 && (tokens.front().type == TType::Ptr || tokens.front().type == TType::Asterisk))
			{
				while(tokens.front().type == TType::Ptr || tokens.front().type == TType::Asterisk)
					eat(tokens), ptrAppend += "*";
			}

			// check if the next token is a '['.
			if(tokens.front().type == TType::LSquare)
			{
				eat(tokens);

				// todo: multidimensional fixed-size arrays.
				Token n = eat(tokens);
				if(n.type != TType::Integer)
					parserError("Expected integer size for fixed-length array");

				std::string dims = n.text;
				n = eat(tokens);
				while(n.type == TType::Comma)
				{
					n = eat(tokens);
					if(n.type == TType::Integer)
					{
						dims += "," + n.text;
						n = eat(tokens);
					}

					else if(n.type == TType::RSquare)
						break;

					else
						parserError("> Unexpected token %s", n.text.c_str());
				}

				ptrAppend += "[" + dims + "]";

				if(n.type != TType::RSquare)
					parserError("Expected ']', have %s", n.text.c_str());
			}

			std::string ret = baseType + ptrAppend;
			Expr* ct = CreateAST(DummyExpr, tmp);
			ct->type.isLiteral = true;
			ct->type.strType = ret;

			return ct;
		}
		else if(tmp.type == TType::Typeof)
		{
			Expr* ct = CreateAST(DummyExpr, tmp);
			ct->type.isLiteral = false;
			ct->type.strType = "__internal_error__";

			ct->type.type = parseExpr(tokens);
			return ct;
		}
		else if(tmp.type == TType::LParen)
		{
			// tuple as a type.
			int parens = 1;

			std::string final = "(";
			while(parens > 0)
			{
				Token front = eat(tokens);
				while(front.type != TType::LParen && front.type != TType::RParen)
				{
					final += front.text;
					front = eat(tokens);
				}

				if(front.type == TType::LParen)
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

			Expr* _dm = parseType(tokens);
			iceAssert(_dm->type.isLiteral);

			DummyExpr* dm = CreateAST(DummyExpr, tmp);
			dm->type.isLiteral = true;
			dm->type.strType = "[" + _dm->type.strType + "]";

			Token next = eat(tokens);
			if(next.type != TType::RSquare)
				parserError("Expected ']' after array type declaration.");

			return dm;
		}
		else
		{
			parserError("Expected type for variable declaration, got %s", tmp.text.c_str());
		}
	}

	VarDecl* parseVarDecl(TokenList& tokens)
	{
		iceAssert(tokens.front().type == TType::Var || tokens.front().type == TType::Val);

		bool immutable = tokens.front().type == TType::Val;
		uint32_t attribs = checkAndApplyAttributes(Attr_NoAutoInit | Attr_VisPublic | Attr_VisInternal | Attr_VisPrivate);

		eat(tokens);

		// get the identifier.
		Token tok_id;
		if((tok_id = eat(tokens)).type != TType::Identifier)
			parserError("Expected identifier for variable declaration.");

		std::string id = tok_id.text;
		VarDecl* v = CreateAST(VarDecl, tok_id, id, immutable);
		v->disableAutoInit = attribs & Attr_NoAutoInit;
		v->attribs = attribs;

		// check the type.
		Token colon = eat(tokens);
		if(colon.type == TType::Colon)
		{
			Expr* ctype = parseType(tokens);
			v->type = ctype->type;

			delete ctype;	// cleanup

			if(tokens.front().type == TType::LParen)
			{
				// this form:
				// var foo: String("bla")

				// since parseFuncCall is actually built for this kind of hack (like with the init() thing)
				// it's easy.
				v->initVal = parseFuncCall(tokens, v->type.strType);
			}
			else if(tokens.front().type == TType::LBrace)
			{
				if(!isParsingStruct)
					parserError("Computed properties can only be declared inside structs");

				// computed property, getting and setting

				// eat the brace, skip whitespace
				ComputedProperty* cprop = CreateAST(ComputedProperty, eat(tokens), id);
				cprop->type = v->type;
				delete v;

				bool didGetter = false;
				bool didSetter = false;
				for(int i = 0; i < 2; i++)
				{
					if(tokens.front().type == TType::Get)
					{
						if(didGetter)
							parserError("Only one getter is allowed per computed property");

						didGetter = true;

						// parse a braced block.
						eat(tokens);
						if(tokens.front().type != TType::LBrace)
							parserError("Expected '{' after 'get'");

						cprop->getter = parseBracedBlock(tokens);
					}
					else if(tokens.front().type == TType::Set)
					{
						if(didSetter)
							parserError("Only one getter is allowed per computed property");

						didSetter = true;

						eat(tokens);
						std::string setValName = "newValue";

						// see if we have parentheses
						if(tokens.front().type == TType::LParen)
						{
							eat(tokens);
							if(tokens.front().type != TType::Identifier)
								parserError("Expected identifier for custom setter argument name");

							setValName = eat(tokens).text;

							if(eat(tokens).type != TType::RParen)
								parserError("Expected closing ')'");
						}

						cprop->setter = parseBracedBlock(tokens);
						cprop->setterArgName = setValName;
					}
					else if(tokens.front().type == TType::RBrace)
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

						tokens.push_front(dummy);
						cprop->getter = parseBracedBlock(tokens);


						// lol, another hack
						dummy.type = TType::RBrace;
						dummy.text = "}";
						tokens.push_front(dummy);
					}
				}

				if(eat(tokens).type != TType::RBrace)
					parserError("Expected closing '}'");

				return cprop;
			}
		}
		else if(colon.type == TType::Equal)
		{
			v->type = "Inferred";

			// make sure the init value parser below works, push the colon back onto the stack
			tokens.push_front(colon);
		}
		else
		{
			parserError("Variable declaration without type requires initialiser for type inference");
		}

		if(!v->initVal)
		{
			if(tokens.front().type == TType::Equal)
			{
				// we do
				eat(tokens);

				v->initVal = parseExpr(tokens);
				if(!v->initVal)
					parserError("Invalid initialiser for variable '%s'", v->name.c_str());
			}
			else if(immutable)
			{
				parserError("Constant variables require an initialiser at the declaration site");
			}
		}

		return v;
	}

	Tuple* parseTuple(TokenList& tokens, Ast::Expr* lhs)
	{
		assert(lhs);

		Token first = tokens.front();
		std::vector<Expr*> values;


		values.push_back(lhs);

		Token t = tokens.front();
		while(true)
		{
			values.push_back(parseExpr(tokens));
			if(tokens.front().type == TType::RParen)
				break;

			else if(tokens.front().type == TType::Comma)
				eat(tokens);

			t = tokens.front();
		}

		// leave the last rparen
		iceAssert(tokens.front().type == TType::RParen);
		// eat(tokens);

		return CreateAST(Tuple, first, values);
	}

	Expr* parseParenthesised(TokenList& tokens)
	{
		iceAssert(eat(tokens).type == TType::LParen);
		didHaveLeftParen = true;
		Expr* within = parseExpr(tokens);

		iceAssert(tokens.front().type == TType::RParen);
		eat(tokens);

		didHaveLeftParen = false;
		return within;
	}

	Expr* parseExpr(TokenList& tokens)
	{
		Expr* lhs = parseUnary(tokens);
		if(!lhs)
			return nullptr;

		return parseRhs(tokens, lhs, 0);
	}

	static Expr* parsePostfixUnaryOp(TokenList& tokens, Token tok, Expr* curLhs)
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
			Expr* inside = parseExpr(tokens);
			if(eat(tokens).type != TType::RSquare)
				parserError("Expected ']' after '[' for array index");

			newlhs = CreateAST(ArrayIndex, top, curLhs, inside);
		}
		else
		{
			iceAssert(false);
		}

		return newlhs;
	}

	Expr* parseRhs(TokenList& tokens, Expr* lhs, int prio)
	{
		while(true)
		{
			int prec = getOpPrec(tokens.front());
			if(prec < prio && !isRightAssociativeOp(tokens.front()))
				return lhs;


			// we don't really need to check, because if it's botched we'll have returned due to -1 < everything
			Token tok_op = eat(tokens);
			if(tok_op.type == TType::Comma && didHaveLeftParen)
			{
				didHaveLeftParen = false;
				return parseTuple(tokens, lhs);
			}
			else if(isPostfixUnaryOperator(tok_op.type))
			{
				lhs = parsePostfixUnaryOp(tokens, tok_op, lhs);
				continue;
			}


			Expr* rhs = (tok_op.type == TType::As) ? parseType(tokens) : parseUnary(tokens);
			if(!rhs)
				return nullptr;

			int next = getOpPrec(tokens.front());

			if(next > prec || isRightAssociativeOp(tokens.front()))
			{
				rhs = parseRhs(tokens, rhs, prec + 1);
				if(!rhs)
					return nullptr;
			}

			currentOperatorPrecedence = getOpPrec(tok_op);

			ArithmeticOp op;
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
				default:					parserError("Unknown operator '%s'", tok_op.text.c_str());
			}

			if(op == ArithmeticOp::MemberAccess)
				lhs = CreateAST(MemberAccess, tok_op, lhs, rhs);

			else
				lhs = CreateAST(BinOp, tok_op, lhs, op, rhs);
		}
	}

	Expr* parseIdExpr(TokenList& tokens)
	{
		Token tok_id = eat(tokens);
		std::string id = tok_id.text;
		VarRef* idvr = CreateAST(VarRef, tok_id, id);

		// check for dot syntax.
		// if(tokens.front().type == TType::LSquare /*&& (currentOperatorPrecedence <= 120)*/)
		// {
		// 	// array dereference

		// 	ArrayIndex* prev_ai = 0;
		// 	while(tokens.front().type == TType::LSquare)
		// 	{
		// 		eat(tokens);
		// 		Expr* within = parseExpr(tokens);

		// 		if(eat(tokens).type != TType::RSquare)
		// 			parserError("Expected ']'");

		// 		auto ai = CreateAST(ArrayIndex, tok_id, idvr, within);

		// 		if(prev_ai)
		// 			prev_ai->arr = ai;

		// 		else
		// 			prev_ai = ai;
		// 	}


		// 	return prev_ai;
		// }
		if(tokens.front().type == TType::LParen)
		{
			delete idvr;
			return parseFuncCall(tokens, id);
		}
		else
		{
			return idvr;
		}
	}

	Alloc* parseAlloc(TokenList& tokens)
	{
		Token tok_alloc = eat(tokens);
		iceAssert(tok_alloc.type == TType::Alloc);

		Alloc* ret = CreateAST(Alloc, tok_alloc);

		if(tokens.front().type == TType::LSquare)
		{
			eat(tokens);
			ret->count = parseExpr(tokens);

			// check for comma, to allocate arrays on the heap
			// ie. let arr = alloc [1, 2, 3].
			// obviously, type is not necessary.
			// probably. if we need to (for polymorphism, to specify the base type, for example)
			// then either

			// alloc: Type [1, 2, 3] or alloc [1, 2, 3]: Type will work.
			// not too hard to implement either.

			if(eat(tokens).type != TType::RSquare)
				parserError("Expected ']' after alloc[num]");
		}

		auto ct = parseType(tokens);
		std::string type = ct->type.strType;
		delete ct;

		if(tokens.front().type == TType::LParen)
		{
			// alloc[...] Foo(...)
			FuncCall* fc = parseFuncCall(tokens, type);
			ret->params = fc->params;
		}

		ret->type = type;

		return ret;
	}

	Dealloc* parseDealloc(TokenList& tokens)
	{
		Token tok_dealloc = eat(tokens);
		iceAssert(tok_dealloc.type == TType::Dealloc);

		Expr* expr = parseExpr(tokens);
		return CreateAST(Dealloc, tok_dealloc, expr);
	}

	Number* parseNumber(TokenList& tokens)
	{
		Number* n;
		if(tokens.front().type == TType::Integer)
		{
			Token tok = eat(tokens);
			n = CreateAST(Number, tok, getIntegerValue(tok));

			// todo: handle integer suffixes
			n->type = "Int64";

			// set the type.
			// always used signed
		}
		else if(tokens.front().type == TType::Decimal)
		{
			Token tok = eat(tokens);
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

	FuncCall* parseFuncCall(TokenList& tokens, std::string id)
	{
		Token front = eat(tokens);
		iceAssert(front.type == TType::LParen);

		std::deque<Expr*> args;

		if(tokens.front().type != TType::RParen)
		{
			while(true)
			{
				Expr* arg = parseExpr(tokens);
				if(arg == nullptr)
					return nullptr;

				args.push_back(arg);
				if(tokens.front().type == TType::RParen)
				{
					eat(tokens);
					break;
				}

				Token t;
				if((t = eat(tokens)).type != TType::Comma)
					parserError("Expected either ',' or ')' in parameter list, got '%s'", t.text.c_str());
			}
		}
		else
		{
			eat(tokens);
		}

		auto ret = CreateAST(FuncCall, front, id, args);
		rootNode->allFunctionCalls.push_back(ret);

		return ret;
	}

	Return* parseReturn(TokenList& tokens)
	{
		Token front = eat(tokens);
		iceAssert(front.type == TType::Return);

		Expr* retval = nullptr;

		// kinda hack: if the next token is a closing brace, then we don't expect an expression
		// this works most of the time.
		if(tokens.front().type != TType::RBrace)
			retval = parseExpr(tokens);

		return CreateAST(Return, front, retval);
	}

	Expr* parseIf(TokenList& tokens)
	{
		Token tok_if = eat(tokens);
		iceAssert(tok_if.type == TType::If);

		typedef std::pair<Expr*, BracedBlock*> CCPair;
		std::deque<CCPair> conds;

		Expr* cond = parseExpr(tokens);
		BracedBlock* tcase = parseBracedBlock(tokens);

		conds.push_back(CCPair(cond, tcase));

		// check for else and else if
		BracedBlock* ecase = nullptr;
		bool parsedElse = false;
		while(tokens.front().type == TType::Else)
		{
			eat(tokens);
			if(tokens.front().type == TType::If && !parsedElse)
			{
				eat(tokens);

				// parse an expr, then a block
				Expr* c = parseExpr(tokens);
				BracedBlock* cl = parseBracedBlock(tokens);

				conds.push_back(CCPair(c, cl));
			}
			else if(!parsedElse)
			{
				parsedElse = true;
				ecase = parseBracedBlock(tokens);
			}
			else
			{
				if(parsedElse && tokens.front().type != TType::If)
				{
					parserError("Duplicate 'else' clause, only one else clause is permitted per if.");
				}
				else
				{
					parserError("The 'else' clause must be the last block in the if statement.");
				}
			}
		}

		return CreateAST(If, tok_if, conds, ecase);
	}

	WhileLoop* parseWhile(TokenList& tokens)
	{
		Token tok_while = eat(tokens);

		if(tok_while.type == TType::While)
		{
			Expr* cond = parseExpr(tokens);
			BracedBlock* body = parseBracedBlock(tokens);

			return CreateAST(WhileLoop, tok_while, cond, body, false);
		}
		else
		{
			iceAssert(tok_while.type == TType::Do || tok_while.type == TType::Loop);

			// parse the block first
			BracedBlock* body = parseBracedBlock(tokens);

			// syntax treat: since a raw block is ignored (for good reason, how can we reference it?)
			// we can use 'do' to run an anonymous block
			// therefore, the 'while' clause at the end is optional; if it's not present, then the condition is false.

			// 'loop' and 'do', when used without the 'while' clause on the end, have opposite behaviours
			// do { ... } runs the block only once, while loop { ... } runs it infinite times.
			// with the 'while' clause, they have the same behaviour.

			Expr* cond = 0;
			if(tokens.front().type == TType::While)
			{
				eat(tokens);
				cond = parseExpr(tokens);
			}
			else
			{
				// here's the magic: continue condition is 'false' for do, 'true' for loop
				cond = CreateAST(BoolVal, tokens.front(), tok_while.type == TType::Do ? false : true);
			}

			return CreateAST(WhileLoop, tok_while, cond, body, true);
		}
	}

	ForLoop* parseFor(TokenList& tokens)
	{
		Token tok_for = eat(tokens);
		iceAssert(tok_for.type == TType::For);

		return 0;
	}

	static StructBase* parseStructBody(TokenList& tokens)
	{
		isParsingStruct = true;
		Token tok_id = eat(tokens);

		std::string id;
		if(tok_id.type != TType::Identifier)
			parserError("Expected identifier");

		id += tok_id.text;
		Struct* str = CreateAST(Struct, tok_id, id);

		uint32_t attr = checkAndApplyAttributes(Attr_PackedStruct | Attr_VisPublic | Attr_VisInternal | Attr_VisPrivate);
		if(attr & Attr_PackedStruct)
			str->packed = true;

		str->attribs = attr;

		// parse a clousure.
		BracedBlock* body = parseBracedBlock(tokens);
		int i = 0;
		for(Expr* stmt : body->statements)
		{
			if(ComputedProperty* cprop = dynamic_cast<ComputedProperty*>(stmt))
			{
				for(ComputedProperty* c : str->cprops)
				{
					if(c->name == cprop->name)
						parserError("Duplicate member '%s'", cprop->name.c_str());
				}

				str->cprops.push_back(cprop);
			}
			else if(VarDecl* var = dynamic_cast<VarDecl*>(stmt))
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
			else if(Func* func = dynamic_cast<Func*>(stmt))
			{
				str->funcs.push_back(func);
				str->typeList.push_back(std::pair<Expr*, int>(func, i));
			}
			else if(OpOverload* oo = dynamic_cast<OpOverload*>(stmt))
			{
				oo->str = str;
				str->opOverloads.push_back(oo);

				str->funcs.push_back(oo->func);
				str->typeList.push_back(std::pair<Expr*, int>(oo, i));
			}
			else if(StructBase* sb = dynamic_cast<StructBase*>(stmt))
			{
				str->nestedTypes.push_back(sb);
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

		isParsingStruct = false;
		delete body;
		return str;
	}




	Struct* parseStruct(TokenList& tokens)
	{
		Token tok_struct = eat(tokens);
		iceAssert(tok_struct.type == TType::Struct);

		Struct* str = CreateAST(Struct, tok_struct, "");
		StructBase* sb = parseStructBody(tokens);

		str->attribs		= sb->attribs;
		str->funcs			= sb->funcs;
		str->opOverloads	= sb->opOverloads;
		str->typeList		= sb->typeList;
		str->members		= sb->members;
		str->nameMap		= sb->nameMap;
		str->name			= sb->name;
		str->nestedTypes	= sb->nestedTypes;
		str->cprops			= sb->cprops;

		delete sb;
		return str;
	}

	Extension* parseExtension(TokenList& tokens)
	{
		Token tok_ext = eat(tokens);
		iceAssert(tok_ext.type == TType::Extension);

		Extension* ext = CreateAST(Extension, tok_ext, "");
		StructBase* str = parseStructBody(tokens);

		ext->attribs		= str->attribs;
		ext->funcs			= str->funcs;
		ext->opOverloads	= str->opOverloads;
		ext->typeList		= str->typeList;
		ext->members		= str->members;
		ext->nameMap		= str->nameMap;
		ext->name			= str->name;
		ext->cprops			= str->cprops;

		delete str;
		return ext;
	}

	Ast::Enumeration* parseEnum(TokenList& tokens)
	{
		iceAssert(eat(tokens).type == TType::Enum);

		Token tok_id;
		if((tok_id = eat(tokens)).type != TType::Identifier)
			parserError("Expected identifier after 'enum'");

		if(eat(tokens).type != TType::LBrace)
			parserError("Expected body after 'enum'");


		if(tokens.front().type == TType::RBrace)
			parserError("Empty enumerations are not allowed");


		Enumeration* enumer = CreateAST(Enumeration, tok_id, tok_id.text);
		Token front = tokens.front();

		uint32_t attr = checkAndApplyAttributes(Attr_StrongTypeAlias | Attr_VisPublic | Attr_VisInternal | Attr_VisPrivate);
		if(attr & Attr_StrongTypeAlias)
			enumer->isStrong = true;

		// parse the stuff.
		bool isFirst = true;
		bool isNumeric = false;
		Number* prevNumber = nullptr;

		while(front = tokens.front(), tokens.size() > 0)
		{
			if(front.type == TType::RBrace && !isFirst)
				break;

			if(front.type != TType::Case)
				parserError("Only 'case' expressions are allowed inside enumerations, got '%s'", front.text.c_str());

			eat(tokens);
			if((front = eat(tokens)).type != TType::Identifier)
				parserError("Expected identifier after 'case', got '%s'", front.text.c_str());

			std::string eName = front.text;
			Expr* value = 0;

			skipNewline(tokens);
			front = tokens.front();
			if(front.type == TType::Equal)
			{
				eat(tokens);
				value = parseExpr(tokens);

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


			skipNewline(tokens);

			front = tokens.front();

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
				eat(tokens);
				break;
			}

		}

		return enumer;
	}

	void parseAttribute(TokenList& tokens)
	{
		iceAssert(eat(tokens).type == TType::At);
		Token id = eat(tokens);

		if(id.type != TType::Identifier && id.type != TType::Private && id.type != TType::Internal && id.type != TType::Public)
			parserError("Expected attribute name after '@'");

		uint32_t attr = 0;
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
		else										parserError("Unknown attribute '%s'", id.text.c_str());

		curAttrib |= attr;
	}

	Break* parseBreak(TokenList& tokens)
	{
		Token tok_br = eat(tokens);
		iceAssert(tok_br.type == TType::Break);

		Break* br = CreateAST(Break, tok_br);
		return br;
	}

	Continue* parseContinue(TokenList& tokens)
	{
		Token tok_cn = eat(tokens);
		iceAssert(tok_cn.type == TType::Continue);

		Continue* cn = CreateAST(Continue, tok_cn);
		return cn;
	}

	Import* parseImport(TokenList& tokens)
	{
		iceAssert(eat(tokens).type == TType::Import);

		std::string s;
		Token tok_mod = tokens.front();
		if(tok_mod.type != TType::Identifier)
			parserError("Expected identifier after import");

		Token t = tok_mod;
		tokens.pop_front();

		while(tokens.size() > 0)
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
			t = tokens.front();
			tokens.pop_front();
		}

		// NOTE: make sure printAst doesn't touch 'cgi', because this will break to hell.
		return CreateAST(Import, tok_mod, s);
	}

	StringLiteral* parseStringLiteral(TokenList& tokens)
	{
		iceAssert(tokens.front().type == TType::StringLiteral);
		Token str = eat(tokens);


		// reference hack in tokeniser.cpp
		str.text = str.text.substr(1);
		auto ret = CreateAST(StringLiteral, str, str.text);

		uint32_t attr = checkAndApplyAttributes(Attr_RawString);
		if(attr & Attr_RawString)
			ret->isRaw = true;

		return ret;
	}

	TypeAlias* parseTypeAlias(TokenList& tokens)
	{
		iceAssert(eat(tokens).type == TType::TypeAlias);
		Token tok_name = eat(tokens);
		if(tok_name.type != TType::Identifier)
			parserError("Expected identifier after 'typealias'");

		if(eat(tokens).type != TType::Equal)
			parserError("Expected '='");


		auto ret = CreateAST(TypeAlias, tok_name, tok_name.text, "");

		Expr* ct = parseType(tokens);
		iceAssert(ct);

		ret->origType = ct->type.strType;
		delete ct;

		uint32_t attr = checkAndApplyAttributes(Attr_StrongTypeAlias);
		if(attr & Attr_StrongTypeAlias)
			ret->isStrong = true;

		return ret;
	}

	DeferredExpr* parseDefer(TokenList& tokens)
	{
		iceAssert(tokens.front().type == TType::Defer);
		return CreateAST(DeferredExpr, eat(tokens), parseExpr(tokens));
	}

	Typeof* parseTypeof(TokenList& tokens)
	{
		iceAssert(tokens.front().type == TType::Typeof);
		return CreateAST(Typeof, eat(tokens), parseExpr(tokens));
	}

	ArrayLiteral* parseArrayLiteral(TokenList& tokens)
	{
		iceAssert(tokens.front().type == TType::LSquare);
		Token front = eat(tokens);

		std::deque<Expr*> values;
		while(true)
		{
			Token tok = tokens.front();
			if(tok.type == TType::Comma)
			{
				eat(tokens);
				continue;
			}
			else if(tok.type == TType::RSquare)
			{
				break;
			}
			else
			{
				values.push_back(parseExpr(tokens));
			}
		}

		iceAssert(tokens.front().type == TType::RSquare);
		eat(tokens);

		return CreateAST(ArrayLiteral, front, values);
	}




	ArithmeticOp mangledStringToOperator(std::string op)
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
		else				parserError("Invalid operator '%s'", op.c_str());
	}

	std::string operatorToMangledString(ArithmeticOp op)
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
			default:								parserError("Invalid operator");
		}
	}

	OpOverload* parseOpOverload(TokenList& tokens)
	{
		if(!isParsingStruct)
			parserError("Can only overload operators in the context of a named aggregate type");

		iceAssert(eat(tokens).text == "operator");
		Token op = eat(tokens);

		ArithmeticOp ao;

		if(op.type == TType::Equal)				ao = ArithmeticOp::Assign;
		else if(op.type == TType::EqualsTo)		ao = ArithmeticOp::CmpEq;
		else if(op.type == TType::Plus)			ao = ArithmeticOp::Add;
		else if(op.type == TType::Minus)		ao = ArithmeticOp::Subtract;
		else if(op.type == TType::Asterisk)		ao = ArithmeticOp::Multiply;
		else if(op.type == TType::Divide)		ao = ArithmeticOp::Divide;

		else if(op.type == TType::PlusEq)		ao = ArithmeticOp::PlusEquals;
		else if(op.type == TType::MinusEq)		ao = ArithmeticOp::MinusEquals;
		else if(op.type == TType::MultiplyEq)	ao = ArithmeticOp::MultiplyEquals;
		else if(op.type == TType::DivideEq)		ao = ArithmeticOp::DivideEquals;
		else									parserError("Unsupported operator overload on operator '%s'", op.text.c_str());

		OpOverload* oo = CreateAST(OpOverload, op, ao);

		Token fake;
		fake.posinfo = currentPos;
		fake.text = "operator#" + operatorToMangledString(ao);
		fake.type = TType::Identifier;

		tokens.push_front(fake);

		// parse a func declaration.
		oo->func = parseFunc(tokens);
		return oo;
	}
}




namespace Ast
{
	uint32_t Attr_Invalid			= 0x00;
	uint32_t Attr_NoMangle			= 0x01;
	uint32_t Attr_VisPublic			= 0x02;
	uint32_t Attr_VisInternal		= 0x04;
	uint32_t Attr_VisPrivate		= 0x08;
	uint32_t Attr_ForceMangle		= 0x10;
	uint32_t Attr_NoAutoInit		= 0x20;
	uint32_t Attr_PackedStruct		= 0x40;
	uint32_t Attr_StrongTypeAlias	= 0x80;
	uint32_t Attr_RawString			= 0x100;
}
















