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

using namespace Ast;


namespace Parser
{
	Token curtok;
	PosInfo currentPos;
	bool isInsideNamespace				= false;
	Root* rootNode						= nullptr;
	uint32_t curAttrib					= 0;
	Codegen::CodegenInstance* curCgi	= nullptr;


	#define CreateAST_Raw(name, ...)		(new name (currentPos, ##__VA_ARGS__))
	#define CreateAST(name, tok, ...)		(new name (tok.posinfo, ##__VA_ARGS__))





	// todo: hack
	bool isParsingStruct;
	void parserError(Token token, const char* msg, va_list args)
	{
		char* alloc = nullptr;
		vasprintf(&alloc, msg, args);

		fprintf(stderr, "%s(%s:%" PRIu64 ")%s Parsing error%s: %s\n\n", COLOUR_BLACK_BOLD, token.posinfo.file.c_str(), token.posinfo.line, COLOUR_RED_BOLD, COLOUR_RESET, alloc);
	}


	// what the fuck
	void parserError(Token token, const char* msg, ...) __attribute__((format(printf, 2, 3)));
	void parserError(Token token, const char* msg, ...)
	{
		va_list ap;
		va_start(ap, msg);

		parserError(token, msg, ap);

		va_end(ap);
		abort();

	}

	// come on man
	void parserError(const char* msg, ...) __attribute__((format(printf, 1, 2)));
	void parserError(const char* msg, ...)
	{
		va_list ap;
		va_start(ap, msg);

		parserError(curtok, msg, ap);

		va_end(ap);
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

		fprintf(stderr, "%s(%s:%" PRIu64 ")%s Warning%s: %s\n\n", COLOUR_BLACK_BOLD, curtok.posinfo.file.c_str(), curtok.posinfo.line, COLOUR_MAGENTA_BOLD, COLOUR_RESET, alloc);

		va_end(ap);

		if(Compiler::getFlag(Compiler::Flag::WarningsAsErrors))
			parserError("Treating warning as error because -Werror was passed");
	}


	// woah shit it's forward declarations
	// note: all these are expected to pop at least one token from the front of the list.

	Expr* parseIf(std::deque<Token>& tokens);
	void parseAll(std::deque<Token>& tokens);
	Func* parseFunc(std::deque<Token>& tokens);
	Expr* parseExpr(std::deque<Token>& tokens);
	Expr* parseUnary(std::deque<Token>& tokens);
	Alloc* parseAlloc(std::deque<Token>& tokens);
	ForLoop* parseFor(std::deque<Token>& tokens);
	Expr* parseIdExpr(std::deque<Token>& tokens);
	Break* parseBreak(std::deque<Token>& tokens);
	Expr* parsePrimary(std::deque<Token>& tokens);
	Expr* parseInitFunc(std::deque<Token>& tokens);
	Struct* parseStruct(std::deque<Token>& tokens);
	Import* parseImport(std::deque<Token>& tokens);
	Return* parseReturn(std::deque<Token>& tokens);
	Number* parseNumber(std::deque<Token>& tokens);
	void parseAttribute(std::deque<Token>& tokens);
	CastedType* parseType(std::deque<Token>& tokens);
	VarDecl* parseVarDecl(std::deque<Token>& tokens);
	WhileLoop* parseWhile(std::deque<Token>& tokens);
	Dealloc* parseDealloc(std::deque<Token>& tokens);
	Continue* parseContinue(std::deque<Token>& tokens);
	Func* parseTopLevelExpr(std::deque<Token>& tokens);
	FuncDecl* parseFuncDecl(std::deque<Token>& tokens);
	Expr* parseParenthesised(std::deque<Token>& tokens);
	OpOverload* parseOpOverload(std::deque<Token>& tokens);
	NamespaceDecl* parseNamespace(std::deque<Token>& tokens);
	BracedBlock* parseBracedBlock(std::deque<Token>& tokens);
	StringLiteral* parseStringLiteral(std::deque<Token>& tokens);
	ForeignFuncDecl* parseForeignFunc(std::deque<Token>& tokens);
	Expr* parseFuncCall(std::deque<Token>& tokens, std::string id);
	Expr* parseRhs(std::deque<Token>& tokens, Expr* expr, int prio);


	std::string getModuleName(std::string filename)
	{
		size_t lastdot = filename.find_last_of(".");
		std::string modname = (lastdot == std::string::npos ? filename : filename.substr(0, lastdot));

		size_t sep = modname.find_last_of("\\/");
		if(sep != std::string::npos)
			modname = modname.substr(sep + 1, modname.length() - sep - 1);

		return modname;
	}

	Root* Parse(std::string filename, std::string str, Codegen::CodegenInstance* cgi)
	{
		curCgi = cgi;

		Token t;
		currentPos.file = filename;
		currentPos.line = 1;
		curAttrib = 0;

		std::deque<Token> tokens;
		std::deque<Token> tokenPtrs;

		while((t = getNextToken(str, currentPos)).text.size() > 0)
		{
			tokens.push_back(t);
			tokenPtrs.push_back(t);
		}

		rootNode = new Root();
		currentPos.file = filename;
		currentPos.line = 1;

		parseAll(tokens);

		return rootNode;
	}

	// helpers
	static void skipNewline(std::deque<Token>& tokens)
	{
		while(tokens.size() > 0 && tokens.front().type == TType::NewLine)
		{
			tokens.pop_front();
			currentPos.line++;
		}
	}

	static Token eat(std::deque<Token>& tokens)
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

	static bool checkHasMore(std::deque<Token>& tokens)
	{
		return tokens.size() > 0;
	}

	static int getOpPrec(Token tok)
	{
		switch(tok.type)
		{
			case TType::DoubleColon:
				return 1000;			// lol

			case TType::As:
			case TType::Period:
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
		}
	}

	int64_t getIntegerValue(Token t)
	{
		assert(t.type == TType::Integer);
		int base = 10;
		if(t.text.find("0x") == 0)
			base = 16;

		return std::stoll(t.text, nullptr, base);
	}

	double getDecimalValue(Token t)
	{
		return std::stod(t.text);
	}

	uint32_t checkAndApplyAttributes(uint32_t allowed)
	{
		static const char* ReadableAttrNames[] =
		{
			"Invalid",
			"NoMangle",
			"Public",
			"Internal",
			"Private",
			"ForceMangle",
			"NoAutoInit",
			"PackedStruct"
		};
		uint32_t disallowed = ~allowed;

		if(curAttrib & disallowed)
		{
			int shifts = 0;
			while((disallowed & 1) == 0)
				disallowed >>= 1, shifts++;

			if(shifts > 0)
				parserError("Invalid attribute '%s' for expression", ReadableAttrNames[shifts]);
		}

		uint32_t ret = curAttrib;
		curAttrib = 0;
		return ret;
	}






	void parseAll(std::deque<Token>& tokens)
	{
		if(tokens.size() == 0)
			return;

		Token tok;
		while(tokens.size() > 0 && (tok = tokens.front()).text.length() > 0)
		{
			switch(tok.type)
			{
				case TType::Func:
					if(!isInsideNamespace) rootNode->topLevelExpressions.push_back(parseFunc(tokens));
					break;

				case TType::Import:
					rootNode->topLevelExpressions.push_back(parseImport(tokens));
					break;

				case TType::ForeignFunc:
					rootNode->topLevelExpressions.push_back(parseForeignFunc(tokens));
					break;

				case TType::Struct:
					if(!isInsideNamespace) rootNode->topLevelExpressions.push_back(parseStruct(tokens));
					break;

				// only at top level
				case TType::Namespace:
					rootNode->topLevelExpressions.push_back(parseNamespace(tokens));
					break;

				// shit you just skip
				case TType::NewLine:
					currentPos.line++;
					// no break

				case TType::Comment:
				case TType::Semicolon:
					tokens.pop_front();
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
					parserError("wtf?");
					break;
			}
		}
	}

	Expr* parseUnary(std::deque<Token>& tokens)
	{
		Token front = tokens.front();

		// check for unary shit
		if(front.type == TType::Exclamation || front.type == TType::Plus || front.type == TType::Minus)
		{
			TType tp = eat(tokens).type;
			ArithmeticOp op = tp == TType::Exclamation ? ArithmeticOp::LogicalNot : (tp == TType::Plus ? ArithmeticOp::Plus : ArithmeticOp::Minus);

			return CreateAST(UnaryOp, front, op, parseUnary(tokens));
		}
		else if(front.type == TType::Deref || front.type == TType::Pound)
		{
			eat(tokens);
			return CreateAST(UnaryOp, front, ArithmeticOp::Deref, parseUnary(tokens));
		}
		else if(front.type == TType::Addr || front.type == TType::Ampersand)
		{
			eat(tokens);
			return CreateAST(UnaryOp, front, ArithmeticOp::AddrOf, parseUnary(tokens));
		}
		else
		{
			return parsePrimary(tokens);
		}
	}

	Expr* parsePrimary(std::deque<Token>& tokens)
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

				case TType::LParen:
					return parseParenthesised(tokens);

				case TType::BuiltinType:
				case TType::Identifier:
					if(tok.text == "init")
						return parseInitFunc(tokens);

					else if(tok.text == "operator")
						return parseOpOverload(tokens);

					return parseIdExpr(tokens);

				case TType::Alloc:
					return parseAlloc(tokens);

				case TType::Dealloc:
					return parseDealloc(tokens);

				case TType::Struct:
					return parseStruct(tokens);

				case TType::At:
					parseAttribute(tokens);
					return parsePrimary(tokens);

				case TType::StringLiteral:
					return parseStringLiteral(tokens);

				case TType::Integer:
				case TType::Decimal:
					return parseNumber(tokens);

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
					// fallthrough

				case TType::Comment:
				case TType::Semicolon:
					eat(tokens);
					return CreateAST(DummyExpr, tok);

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
					break;
			}
		}

		return nullptr;
	}


	NamespaceDecl* parseNamespace(std::deque<Token>& tokens)
	{
		Token tok_ns = eat(tokens);
		assert(tok_ns.type == TType::Namespace);

		// parse an identifier
		Token front = tokens.front();
		std::deque<std::string> scopes;

		while((front = eat(tokens)).text.length() > 0 && front.type != TType::LBrace)
		{
			if(front.type == TType::Identifier)
				scopes.push_back(front.text);

			else if(front.type == TType::DoubleColon)
				continue;

			else
				parserError("Unexpected token '%s'", front.text.c_str());
		}

		tokens.push_front(front);
		assert(tokens.front().type == TType::LBrace);

		bool wasInsideNamespace = isInsideNamespace;

		isInsideNamespace = true;
		BracedBlock* inside = parseBracedBlock(tokens);
		isInsideNamespace = wasInsideNamespace;

		return CreateAST(NamespaceDecl, tok_ns, scopes, inside);
	}




	FuncDecl* parseFuncDecl(std::deque<Token>& tokens)
	{
		// todo: better things? it's right now mostly hacks.
		if(tokens.front().text != "init" && tokens.front().text.find("operator") != 0)
			assert(eat(tokens).type == TType::Func);

		if(tokens.front().type != TType::Identifier)
			parserError("Expected identifier, but got token of type %d", tokens.front().type);

		Token func_id = eat(tokens);
		std::string id = func_id.text;

		// expect a left bracket
		Token paren = eat(tokens);
		if(paren.type != TType::LParen)
			parserError("Expected '(' in function declaration, got '%s'", paren.text.c_str());

		bool isVA = false;
		// get the parameter list
		// expect an identifer, colon, type
		std::deque<VarDecl*> params;
		std::map<std::string, VarDecl*> nameCheck;

		while(tokens.size() > 0 && tokens.front().type != TType::RParen)
		{
			Token tok_id;
			if((tok_id = eat(tokens)).type != TType::Identifier)
			{
				if(tok_id.type == TType::Elipsis)
				{
					isVA = true;
					if(tokens.front().type != TType::RParen)
						parserError("Vararg declaration must be last in the function declaration");

					break;
				}
				else
				{
					parserError("Expected identifier");
				}
			}

			std::string id = tok_id.text;
			VarDecl* v = CreateAST(VarDecl, tok_id, id, true);

			// expect a colon
			if(eat(tokens).type != TType::Colon)
				parserError("Expected ':' followed by a type");

			CastedType* ctype = parseType(tokens);
			v->type = ctype->name;
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
			CastedType* ctype = parseType(tokens);
			ret = ctype->name;
			delete ctype;
		}
		else
		{
			ret = "Void";
		}

		skipNewline(tokens);
		FuncDecl* f = CreateAST(FuncDecl, func_id, id, params, ret);
		f->attribs = checkAndApplyAttributes(Attr_VisPublic | Attr_VisInternal | Attr_VisPrivate | Attr_NoMangle | Attr_ForceMangle);

		f->hasVarArg = isVA;
		return f;
	}

	ForeignFuncDecl* parseForeignFunc(std::deque<Token>& tokens)
	{
		Token func = tokens.front();
		assert(func.type == TType::ForeignFunc);
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

	BracedBlock* parseBracedBlock(std::deque<Token>& tokens)
	{
		Token tok_cls = eat(tokens);
		BracedBlock* c = CreateAST(BracedBlock, tok_cls);

		// make sure the first token is a left brace.
		if(tok_cls.type != TType::LBrace)
			parserError("Expected '{' to begin a block, found '%s'!", tok_cls.text.c_str());

		// get the stuff inside.
		while(tokens.size() > 0 && tokens.front().type != TType::RBrace)
		{
			c->statements.push_back(parseExpr(tokens));
			skipNewline(tokens);
		}

		if(eat(tokens).type != TType::RBrace)
			parserError("Expected '}'");

		return c;
	}

	Func* parseFunc(std::deque<Token>& tokens)
	{
		Token front = tokens.front();
		FuncDecl* decl = parseFuncDecl(tokens);

		return CreateAST(Func, front, decl, parseBracedBlock(tokens));
	}


	Expr* parseInitFunc(std::deque<Token>& tokens)
	{
		Token front = tokens.front();
		assert(front.text == "init");

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


















	CastedType* parseType(std::deque<Token>& tokens)
	{
		bool isArr = false;
		int arrsize = 0;
		Token tmp = eat(tokens);

		if(tmp.type != TType::Identifier && tmp.type != TType::BuiltinType)
			parserError("Expected type for variable declaration");

		std::string baseType = tmp.text;

		// parse until we get a non-identifier and non-scoperes
		{
			Token t;
			bool expectingScope = true;
			while((t = tokens.front()).text.length() > 0)
			{
				if(t.type == TType::DoubleColon && expectingScope)
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
			}
		}

		std::string ptrAppend = "";
		if(tokens.size() > 0)
		{
			if(tokens.front().type == TType::Ptr || tokens.front().type == TType::Asterisk)
			{
				while(tokens.front().type == TType::Ptr || tokens.front().type == TType::Asterisk)
					eat(tokens), ptrAppend += "*";
			}
			else if(tokens.front().type == TType::LSquare)
			{
				isArr = true;
				eat(tokens);

				Token next = eat(tokens);
				if(next.type == TType::Integer)
					arrsize = getIntegerValue(next), next = eat(tokens);

				if(next.type != TType::RSquare)
					parserError("Expected either constant integer or ']' after array declaration and '['");
			}
		}

		std::string ret = baseType + ptrAppend + (isArr ? "[" + std::to_string(arrsize) + "]" : "");
		return CreateAST(CastedType, tmp, ret);
	}

	VarDecl* parseVarDecl(std::deque<Token>& tokens)
	{
		assert(tokens.front().type == TType::Var || tokens.front().type == TType::Val);

		bool immutable = tokens.front().type == TType::Val;
		bool noautoinit = checkAndApplyAttributes(Attr_NoAutoInit) > 0;

		eat(tokens);

		// get the identifier.
		Token tok_id;
		if((tok_id = eat(tokens)).type != TType::Identifier)
			parserError("Expected identifier for variable declaration.");

		std::string id = tok_id.text;
		VarDecl* v = CreateAST(VarDecl, tok_id, id, immutable);
		v->disableAutoInit = noautoinit;

		// check the type.
		// todo: type inference
		Token colon = eat(tokens);
		if(colon.type == TType::Colon)
		{
			CastedType* ctype = parseType(tokens);
			v->type = ctype->name;

			delete ctype;	// cleanup

			if(tokens.front().type == TType::LParen)
			{
				// this form:
				// var foo: String("bla")

				// since parseFuncCall is actually built for this kind of hack (like with the init() thing)
				// it's easy.
				v->initVal = parseFuncCall(tokens, v->type);
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

	Expr* parseParenthesised(std::deque<Token>& tokens)
	{
		assert(eat(tokens).type == TType::LParen);
		Expr* within = parseExpr(tokens);

		if(eat(tokens).type != TType::RParen)
			parserError("Expected ')'");

		return within;
	}

	Expr* parseExpr(std::deque<Token>& tokens)
	{
		Expr* lhs = parseUnary(tokens);
		if(!lhs)
			return nullptr;

		return parseRhs(tokens, lhs, 0);
	}

	Expr* parseRhs(std::deque<Token>& tokens, Expr* lhs, int prio)
	{
		while(true)
		{
			int prec = getOpPrec(tokens.front());
			if(prec < prio)
				return lhs;


			// we don't really need to check, because if it's botched we'll have returned due to -1 < everything
			Token tok_op = eat(tokens);

			Expr* rhs = tok_op.type == TType::As ? parseType(tokens) : parseUnary(tokens);
			if(!rhs)
				return nullptr;

			int next = getOpPrec(tokens.front());
			if(prec < next)
			{
				rhs = parseRhs(tokens, rhs, prec + 1);
				if(!rhs)
					return nullptr;
			}

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
				case TType::As:				op = ArithmeticOp::Cast;				break;

				case TType::PlusEq:			op = ArithmeticOp::PlusEquals;			break;
				case TType::MinusEq:		op = ArithmeticOp::MinusEquals;			break;
				case TType::MultiplyEq:		op = ArithmeticOp::MultiplyEquals;		break;
				case TType::DivideEq:		op = ArithmeticOp::DivideEquals;		break;
				case TType::ModEq:			op = ArithmeticOp::ModEquals;			break;
				case TType::ShiftLeftEq:	op = ArithmeticOp::ShiftLeftEquals;		break;
				case TType::ShiftRightEq:	op = ArithmeticOp::ShiftRightEquals;	break;
				case TType::Period:			op = ArithmeticOp::MemberAccess;		break;
				case TType::DoubleColon:	op = ArithmeticOp::ScopeResolution;		break;
				default:					parserError("Unknown operator '%s'", tok_op.text.c_str());
			}

			if(op == ArithmeticOp::ScopeResolution)
				lhs = CreateAST(ScopeResolution, tok_op, lhs, rhs);

			else
				lhs = CreateAST(BinOp, tok_op, lhs, op, rhs);
		}
	}

	Expr* parseIdExpr(std::deque<Token>& tokens)
	{
		Token tok_id = eat(tokens);
		std::string id = tok_id.text;
		VarRef* idvr = CreateAST(VarRef, tok_id, id);

		// check for dot syntax.
		if(tokens.front().type == TType::LSquare)
		{
			// array dereference
			eat(tokens);
			Expr* within = parseExpr(tokens);

			if(eat(tokens).type != TType::RSquare)
				parserError("Expected ']'");

			return CreateAST(ArrayIndex, tok_id, idvr, within);
		}
		else if(tokens.front().type == TType::LParen)
		{
			delete idvr;
			return parseFuncCall(tokens, id);
		}
		else
		{
			return idvr;
		}
	}

	Alloc* parseAlloc(std::deque<Token>& tokens)
	{
		Token tok_alloc = eat(tokens);
		assert(tok_alloc.type == TType::Alloc);

		Alloc* ret = CreateAST(Alloc, tok_alloc);

		if(tokens.front().type == TType::LParen)
		{
			eat(tokens);
			ret->count = parseExpr(tokens);
			if(eat(tokens).type != TType::RParen)
				parserError("Expected ')' after alloc(num)");
		}

		Expr* type = parseIdExpr(tokens);
		VarRef* vr = dynamic_cast<VarRef*>(type);
		FuncCall* fc = dynamic_cast<FuncCall*>(type);

		if(vr)
		{
			ret->type = vr->name;
		}
		else if(fc)
		{
			ret->type = fc->name;
			ret->params = fc->params;
		}
		else
		{
			parserError("What?!");
		}

		return ret;
	}

	Dealloc* parseDealloc(std::deque<Token>& tokens)
	{
		Token tok_dealloc = eat(tokens);
		assert(tok_dealloc.type == TType::Dealloc);

		Token tok_id = eat(tokens);
		if(tok_id.type != TType::Identifier)
			parserError("Expected identifier after 'dealloc'");

		VarRef* vr = CreateAST(VarRef, tok_dealloc, tok_id.text);
		return CreateAST(Dealloc, tok_dealloc, vr);
	}

	Number* parseNumber(std::deque<Token>& tokens)
	{
		Number* n;
		if(tokens.front().type == TType::Integer)
		{
			Token tok = eat(tokens);
			n = CreateAST(Number, tok, getIntegerValue(tok));

			// todo: handle integer suffixes
			if(n->ival > INT_MAX)
				n->type = "Int64";

			else
				n->type = "Int32";

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
			assert(false);
			return nullptr;
		}

		return n;
	}

	Expr* parseFuncCall(std::deque<Token>& tokens, std::string id)
	{
		Token front = eat(tokens);
		assert(front.type == TType::LParen);

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

		return CreateAST(FuncCall, front, id, args);
	}

	Return* parseReturn(std::deque<Token>& tokens)
	{
		Token front = eat(tokens);
		assert(front.type == TType::Return);

		return CreateAST(Return, front, parseExpr(tokens));
	}

	Expr* parseIf(std::deque<Token>& tokens)
	{
		Token tok_if = eat(tokens);
		assert(tok_if.type == TType::If);

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

	WhileLoop* parseWhile(std::deque<Token>& tokens)
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
			assert(tok_while.type == TType::Do || tok_while.type == TType::Loop);

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

	ForLoop* parseFor(std::deque<Token>& tokens)
	{
		Token tok_for = eat(tokens);
		assert(tok_for.type == TType::For);

		return 0;
	}

	Struct* parseStruct(std::deque<Token>& tokens)
	{
		Token tok_struct = eat(tokens);
		assert(tok_struct.type == TType::Struct);
		isParsingStruct = true;

		// get the identifier (name)
		std::string id;
		if(tokens.front().type != TType::Identifier)
			parserError("Expected name after 'struct'");

		id += eat(tokens).text;
		Struct* str = CreateAST(Struct, tok_struct, id);

		uint32_t attr = checkAndApplyAttributes(Attr_PackedStruct | Attr_VisPublic | Attr_VisInternal | Attr_VisPrivate);
		if(attr & Attr_PackedStruct)
			str->packed = true;

		// parse a clousure.
		BracedBlock* body = parseBracedBlock(tokens);
		int i = 0;
		for(Expr* stmt : body->statements)
		{
			// check for top-level statements
			VarDecl* var = nullptr;
			Func* func = nullptr;
			OpOverload* oo = nullptr;

			if((var = dynamic_cast<VarDecl*>(stmt)))
			{
				if(str->nameMap.find(var->name) != str->nameMap.end())
					parserError("Duplicate member '%s'", var->name.c_str());

				str->members.push_back(var);
				str->nameMap[var->name] = i;

				i++;
			}
			else if((func = dynamic_cast<Func*>(stmt)))
			{
				str->funcs.push_back(func);
				str->typeList.push_back(std::pair<Expr*, int>(func, i));
			}
			else if((oo = dynamic_cast<OpOverload*>(stmt)))
			{
				oo->str = str;
				str->opOverloads.push_back(oo);

				str->funcs.push_back(oo->func);
				str->typeList.push_back(std::pair<Expr*, int>(oo, i));
			}
			else if(dynamic_cast<DummyExpr*>(stmt))
			{
				continue;
			}
			else
			{
				parserError("Only variable and function declarations are allowed in structs, got %s", typeid(*stmt).name());
			}
		}

		return str;
	}

	void parseAttribute(std::deque<Token>& tokens)
	{
		assert(eat(tokens).type == TType::At);
		Token id = eat(tokens);

		if(id.type != TType::Identifier)
			parserError("Expected attribute name after '@'");

		uint32_t attr = 0;
		if(id.text == "nomangle")				attr |= Attr_NoMangle;
		else if(id.text == "forcemangle")		attr |= Attr_ForceMangle;
		else if(id.text == "noautoinit")		attr |= Attr_NoAutoInit;
		else if(id.text == "packed")			attr |= Attr_PackedStruct;
		else									parserError("Unknown attribute '%s'", id.text.c_str());

		curAttrib |= attr;
	}

	Break* parseBreak(std::deque<Token>& tokens)
	{
		Token tok_br = eat(tokens);
		assert(tok_br.type == TType::Break);

		Break* br = CreateAST(Break, tok_br);
		return br;
	}

	Continue* parseContinue(std::deque<Token>& tokens)
	{
		Token tok_cn = eat(tokens);
		assert(tok_cn.type == TType::Continue);

		Continue* cn = CreateAST(Continue, tok_cn);
		return cn;
	}

	Import* parseImport(std::deque<Token>& tokens)
	{
		assert(eat(tokens).type == TType::Import);

		Token tok_mod;
		if((tok_mod = eat(tokens)).type != TType::Identifier)
			parserError("Expected module name after 'import' statement.");

		return CreateAST(Import, tok_mod, tok_mod.text);
	}

	StringLiteral* parseStringLiteral(std::deque<Token>& tokens)
	{
		assert(tokens.front().type == TType::StringLiteral);
		Token str = eat(tokens);

		return CreateAST(StringLiteral, str, str.text);
	}

	OpOverload* parseOpOverload(std::deque<Token>& tokens)
	{
		if(!isParsingStruct)
			parserError("Can only overload operators in the context of a named aggregate type");

		assert(eat(tokens).text == "operator");
		Token op = eat(tokens);

		ArithmeticOp ao;
		switch(op.type)
		{
			case TType::Equal:
				ao = ArithmeticOp::Assign;
				break;

			case TType::EqualsTo:
				ao = ArithmeticOp::CmpEq;
				break;

			default:
				parserError("Unsupported operator overload on operator '%s'", op.text.c_str());
		}

		OpOverload* oo = CreateAST(OpOverload, op, ao);

		Token fake;
		fake.posinfo = currentPos;
		fake.text = "operator#" + op.text;
		fake.type = TType::Identifier;

		tokens.push_front(fake);

		// parse a func declaration.
		oo->func = parseFunc(tokens);
		return oo;
	}
}




namespace Ast
{
	uint32_t Attr_Invalid		= 0x00;
	uint32_t Attr_NoMangle		= 0x01;
	uint32_t Attr_VisPublic		= 0x02;
	uint32_t Attr_VisInternal	= 0x04;
	uint32_t Attr_VisPrivate	= 0x08;
	uint32_t Attr_ForceMangle	= 0x10;
	uint32_t Attr_NoAutoInit	= 0x20;
	uint32_t Attr_PackedStruct	= 0x40;
}
















