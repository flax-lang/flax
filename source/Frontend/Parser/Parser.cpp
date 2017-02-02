// Parser.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include <map>
#include <cfloat>
#include <fstream>
#include <cassert>
#include <cinttypes>
#include <algorithm>

#include "pts.h"
#include "ast.h"
#include "parser.h"
#include "compiler.h"
#include "codegen.h"

using namespace Ast;

namespace Parser
{
	#define CreateAST_Pin(name, pin, ...)	(new name (pin, ##__VA_ARGS__))
	#define CreateAST(name, tok, ...)		(new name (tok.pin, ##__VA_ARGS__))

	#define CreateASTPos(name, f, l, c, len, ...)	(new name (Parser::Pin(f, l, c, len), ##__VA_ARGS__))


	#define ATTR_STR_NOMANGLE			"nomangle"
	#define ATTR_STR_FORCEMANGLE		"forcemangle"
	#define ATTR_STR_NOAUTOINIT			"noinit"
	#define ATTR_STR_PACKEDSTRUCT		"packed"
	#define ATTR_STR_STRONG				"strong"
	#define ATTR_STR_RAW				"raw"
	#define ATTR_STR_OPERATOR			"operator"

	static ParserState* staticState = 0;

	void setStaticState(ParserState& ps)
	{
		staticState = &ps;
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

	static std::map<std::string, TypeConstraints_t> parseGenericTypeList(ParserState& ps);

	static bool isRightAssociativeOp(Token tok)
	{
		return false;
	}

	static bool isPostfixUnaryOperator(TType tt)
	{
		return (tt == TType::LSquare) || (tt == TType::DoublePlus) || (tt == TType::DoubleMinus);
	}

	static int getOpPrecForUnaryOp(ArithmeticOp op)
	{
		switch(op)
		{
			case ArithmeticOp::LogicalNot:
			case ArithmeticOp::Plus:
			case ArithmeticOp::Minus:
			case ArithmeticOp::BitwiseNot:
			case ArithmeticOp::Deref:
			case ArithmeticOp::AddrOf:
				return 950;

			default:
				return -1;
		}
	}

	static int getCurOpPrec(ParserState& ps)
	{
		// handle >>, >>=, <<, <<=.
		if(ps.getRemainingTokens() > 1 && (ps.front().type == TType::LAngle || ps.front().type == TType::RAngle))
		{
			// check if the next one matches.
			if(ps.front().type == TType::LAngle && ps.lookahead(1).type == TType::LAngle)
				return 650;

			else if(ps.front().type == TType::RAngle && ps.lookahead(1).type == TType::RAngle)
				return 650;


			else if(ps.front().type == TType::LAngle && ps.lookahead(1).type == TType::LessThanEquals)
				return 100;

			else if(ps.front().type == TType::RAngle && ps.lookahead(1).type == TType::GreaterEquals)
				return 100;
		}

		// note that unary ops have precedence handled separately
		switch(ps.front().type)
		{
			// case TType::Comma:
				// return ps.leftParenNestLevel > 0 ? 9001 : -1;	// lol x3

			// . and [] have the same precedence.
			// not sure if this should stay -- works for now.
			case TType::Period:
			case TType::LSquare:
				return 1000;

			// unary !
			// unary +/-
			// bitwise ~
			// unary &
			// unary *
			case TType::As:
				return 900;

			case TType::DoublePlus:
			case TType::DoubleMinus:
				return 850;

			case TType::Asterisk:
			case TType::Divide:
			case TType::Percent:
				return 800;

			case TType::Plus:
			case TType::Minus:
				return 750;

			// << and >>
			// precedence = 700


			case TType::Ampersand:
				return 650;

			case TType::Caret:
				return 600;

			case TType::Pipe:
				return 550;

			case TType::LAngle:
			case TType::RAngle:
			case TType::LessThanEquals:
			case TType::GreaterEquals:
				return 500;

			case TType::EqualsTo:
			case TType::NotEquals:
				return 450;

			case TType::LogicalAnd:
				return 400;

			case TType::LogicalOr:
				return 350;

			case TType::Equal:
			case TType::PlusEq:
			case TType::MinusEq:
			case TType::MultiplyEq:
			case TType::DivideEq:
			case TType::ModEq:
			case TType::AmpersandEq:
			case TType::PipeEq:
			case TType::CaretEq:
				return 100;


			case TType::ShiftLeftEq:
			case TType::ShiftRightEq:
			case TType::ShiftLeft:
			case TType::ShiftRight:
				iceAssert(0);	// note: handled above, should not reach here
				break;

			case TType::Identifier:
			case TType::UnicodeSymbol:
				if(ps.cgi->customOperatorMapRev.find(ps.front().text.to_string()) != ps.cgi->customOperatorMapRev.end())
				{
					return ps.cgi->customOperatorMap[ps.cgi->customOperatorMapRev[ps.front().text.to_string()]].second;
				}
				return -1;

			default:
				return -1;
		}
	}











	// this is stupid, but we need to return references (for string_view reasons)
	static std::string _lookupTable[(size_t) ArithmeticOp::UserDefined] = {
		"+", "-", "*", "/", "%", "<<", ">>", "=", "<", ">", "<=", ">=", "==", "!=",
		"!", "+", "-", "&", "#", "&", "|", "^", "~", "&&", "||", "as", "as!", "+=",
		"-=", "*=", "/=", "%=", "<<=", ">>=", "&=", "|=", "^=", ".", "::", ",", "[]",
	};


	const std::string& arithmeticOpToString(Codegen::CodegenInstance* cgi, Ast::ArithmeticOp op)
	{
		if(op < ArithmeticOp::UserDefined && op != ArithmeticOp::Invalid)
		{
			return _lookupTable[(size_t) op];
		}
		else if(cgi->customOperatorMap.find(op) != cgi->customOperatorMap.end())
		{
			return cgi->customOperatorMap[op].first;
		}
		else
		{
			parserError("Invalid arithmetic operator");
		}
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



	void parseAllCustomOperators(Codegen::CodegenInstance* cgi, std::string filename, std::string curpath)
	{
		Parser::ParserState ps(cgi, Compiler::getFileTokens(Compiler::getFullPathOfFile(filename)));

		staticState = &ps;

		// split into lines
		ps.currentPos.fileID = Compiler::getFileIDFromFilename(filename);

		ps.currentPos.line = 1;
		ps.currentPos.col = 1;


		// todo: hacks
		ps.leftParenNestLevel = 0;
		ps.structNestLevel = 0;
		ps.currentOpPrec = 0;

		ps.skipNewline();


		// hackjob... kinda.
		{
			int curPrec = 0;
			while(ps.hasTokens() > 0)
			{
				Token t = ps.front();

				if(t.type == TType::Import)
				{
					Import* imp = parseImport(ps);
					std::string file = Compiler::resolveImport(imp, Compiler::getFullPathOfFile(filename));

					if(ps.visited.find(file) == ps.visited.end())
					{
						ps.visited.insert(file);

						parseAllCustomOperators(ps.cgi, file, curpath);
					}
				}
				else if(t.type == TType::At)
				{
					// remove the '@'
					ps.pop();

					Token attr = ps.pop();

					iceAssert(attr.type == TType::Identifier || attr.type == TType::Operator);

					if(attr.text == ATTR_STR_OPERATOR)
					{
						ps.skipNewline();
						if(ps.front().type != TType::LSquare)
							parserError(ps.front(), "Expected '[' after @operator");

						ps.pop();
						ps.skipNewline();

						Token num = ps.pop();
						ps.skipNewline();

						// todo: a bit messy
						if(num.type == TType::Identifier)
						{
							// skip.
							if(ps.front().type == TType::RSquare)
							{
								ps.pop();
								curPrec = 0;
								continue; // break out of the loopy
							}
							else if(ps.front().type == TType::Comma)
							{
								ps.pop();
								num = ps.front();
							}
							else
							{
								parserError(ps.front(), "Expected either ']' or ',' after identifier in @operator");
							}
						}


						if(num.type != TType::Number)
							parserError(num, "Expected integer as first attribute within @operator[]");

						curPrec = std::stoi(num.text.to_string());
						if(curPrec <= 0)
							parserError(num, "Precedence must be greater than 0");

						ps.skipNewline();


						// Commutative
						if(ps.front().type == TType::Comma)
						{
							ps.pop();
							if(ps.eat().type != TType::Identifier)
								parserError(ps.front(), "Expected identifier after comma");
						}


						if(ps.front().type != TType::RSquare)
							parserError(ps.front(), "Expected closing ']'");


						ps.pop();
						ps.skipNewline();
					}
				}
				else if(t.type == TType::Operator)
				{
					// eat the 'operator'
					ps.pop();

					ps.skipNewline();
					Token op = ps.front();

					if(op.type == TType::Identifier || op.type == TType::UnicodeSymbol)
					{
						size_t opNum = ps.cgi->customOperatorMap.size();

						if(curPrec <= 0)
							parserError(t, "Custom operators must have a precedence, use @operator[x]");

						// check if it exists.
						auto txt = op.text.to_string();
						if(ps.cgi->customOperatorMapRev.find(txt) == ps.cgi->customOperatorMapRev.end())
						{
							ps.cgi->customOperatorMap[(ArithmeticOp) ((size_t) ArithmeticOp::UserDefined + opNum)] = { txt, curPrec };
							ps.cgi->customOperatorMapRev[txt] = (ArithmeticOp) ((size_t) ArithmeticOp::UserDefined + opNum);
						}
						else
						{
							ArithmeticOp ao = ps.cgi->customOperatorMapRev[txt];
							auto pair = ps.cgi->customOperatorMap[ao];

							if(pair.second != curPrec)
							{
								parserMessage(Err::Warn, op, "Operator '%s' was previously defined with a different precedence (%d)."
									"Due to the way the flax compiler is engineered, all custom operators using the same identifier will be"
									"bound to the first precedence defined.", pair.first.c_str(), pair.second);
							}
						}

						curPrec = 0;
					}
				}
				else if(t.type == TType::Private || t.type == TType::Internal || t.type == TType::Public)
				{
					// eat the thing.
					ps.pop();

					switch(t.type)
					{
						case TType::Private:	ps.curAttrib |= Attr_VisPrivate; break;
						case TType::Internal:	ps.curAttrib |= Attr_VisInternal; break;
						case TType::Public:		ps.curAttrib |= Attr_VisPublic; break;

						default: iceAssert(0);
					}
				}
				else if(curPrec > 0)
				{
					parserError(ps.front(), "@operator can only be applied to operators (%s)", ps.front().text.to_string().c_str());
				}
				else
				{
					// skip it regardless.
					ps.pop();
				}
			}
		}
	}

	Root* Parse(Codegen::CodegenInstance* cgi, std::string filename)
	{
		auto p = prof::Profile("parse");
		ParserState ps(cgi, Compiler::getFileTokens(filename));

		ps.rootNode = new Root();

		ps.currentPos.fileID = Compiler::getFileIDFromFilename(filename);
		ps.currentPos.line = 1;
		ps.currentPos.col = 1;


		// todo: hacks
		ps.currentOpPrec = 0;
		ps.structNestLevel = 0;
		ps.leftParenNestLevel = 0;

		ps.skipNewline();

		staticState = &ps;

		parseAll(ps);
		return ps.rootNode;
	}


	// this only handles the topmost level.
	void parseAll(ParserState& ps)
	{
		while(ps.hasTokens())
		{
			auto expr = parseStatement(ps, true);
			if(!expr) break;

			ps.rootNode->topLevelExpressions.push_back(expr);
		}
	}



	static void _skipWhitespaceAndComments(ParserState& ps)
	{
		// skip whitespace and comments
		while(ps.front().type == TType::Comment || ps.front().type == TType::NewLine)
			ps.pop();
	}

	Expr* parseStatement(ParserState& ps, bool allowingImports)
	{
		if(ps.empty())
			parserError("Unexpected end of file");

		_skipWhitespaceAndComments(ps);

		// todo/hack: parseBracedBlock expects the closing brace to still be there, but we expect a statement of some kind
		// if the last token in a block was a comment, we'll have skipped it, leaving a RBrace in the front now.

		// also EOF just return 0.

		if(ps.front().type == TType::RBrace || ps.front().type == TType::EndOfFile)
			return 0;


		bool skipCheck = false;
		Expr* ret = 0;

		// note: labels!
		// reason we do this (using goto) is because we want to be able to skip the check for newline/semicolon
		// if we use recursion, we'd have to pass skipCheck as a flag or something, which IMO is less elegant than a
		// well-planned goto.

		// Without this fix, the statement itself checks for the newline (and removes it), then when it exists, the attribute/public/private
		// parsing expects another newline, and dies.
		again:

		// for good measure
		// note: actually to allow attributes on separate lines
		// but that's a good measure right?
		_skipWhitespaceAndComments(ps);

		Token tok = ps.front();
		switch(tok.type)
		{
			case TType::Var:
			case TType::Val:
				ret = parseVarDecl(ps);
				break;

			case TType::Func:
				ret = parseFunc(ps);
				break;

			case TType::ForeignFunc:
				ret = parseForeignFunc(ps);
				break;

			case TType::Operator:
				ret = parseOpOverload(ps);
				skipCheck = true;
				break;

			case TType::Static:
				ret = parseStaticDecl(ps);
				skipCheck = true;
				break;

			case TType::Struct:
				ret = parseStruct(ps);
				skipCheck = true;
				break;

			case TType::Class:
				ret = parseClass(ps);
				skipCheck = true;
				break;

			case TType::Protocol:
				ret = parseProtocol(ps);
				skipCheck = true;
				break;

			case TType::Enum:
				ret = parseEnum(ps);
				skipCheck = true;
				break;

			case TType::Extension:
				ret = parseExtension(ps);
				skipCheck = true;
				break;

			case TType::If:
				ret = parseIf(ps);
				skipCheck = true;
				break;

			// all have the same kind of AST node
			case TType::Do:
			case TType::While:
			case TType::Loop:
				ret = parseWhile(ps);
				skipCheck = true;
				break;

			case TType::Namespace:
				ret = parseNamespace(ps);
				skipCheck = true;
				break;


			case TType::Dealloc:
				ret = parseDealloc(ps);
				break;

			case TType::Defer:
				ret = parseDefer(ps);
				break;

			case TType::Return:
				ret = parseReturn(ps);
				break;

			case TType::Break:
				ret = parseBreak(ps);
				break;

			case TType::Continue:
				ret = parseContinue(ps);
				break;

			// skip this shit
			case TType::Semicolon:
				ps.pop();
				ret = CreateAST(DummyExpr, tok);
				break;

			case TType::TypeAlias:
				ret = parseTypeAlias(ps);
				break;



			case TType::At:
				parseAttribute(ps);
				skipCheck = true;
				goto again;
				break;

			case TType::Private:
				ps.pop();
				ps.curAttrib |= Attr_VisPrivate;
				skipCheck = true;
				goto again;
				break;

			case TType::Internal:
				ps.pop();
				ps.curAttrib |= Attr_VisInternal;
				skipCheck = true;
				goto again;
				break;

			case TType::Public:
				ps.pop();
				ps.curAttrib |= Attr_VisPublic;
				skipCheck = true;
				goto again;
				break;

			case TType::Override:
				ps.pop();
				ps.curAttrib |= Attr_Override;
				skipCheck = true;
				goto again;
				break;

			case TType::LBrace:
				// parse it, but throw it away
				ret = CreateAST(DummyExpr, ps.front());
				parserMessage(Err::Warn, parseBracedBlock(ps)->pin, "Anonymous blocks are ignored; to execute, preface with 'do'");
				break;

			case TType::Import:
				if(allowingImports)	ret = parseImport(ps);
				else				parserError("Imports are only allowed in the global scope");
				break;

			default:
				ret = parseExpr(ps);
				break;
		}

		// parserMessage(Err::Warn, "end of statement: %s / %d\n", ps.front().text.to_string().c_str(), ps.front().type);

		iceAssert(ret);
		if(!skipCheck)
		{
			auto t = ps.front().type;
			if(t != TType::NewLine && t != TType::Comment && t != TType::RBrace && t != TType::Semicolon)
			{
				parserError(ret->pin, "Expected newline or semicolon to terminate a statement, found '%s' (id = %d)",
					ps.front().text.to_string().c_str(), t);
			}

			// keep it in there
			if(t != TType::RBrace)
				ps.pop();
		}

		return ret;
	}





	Expr* parsePrimary(ParserState& ps)
	{
		if(ps.empty())
			parserError("Unexpected end of file");

		_skipWhitespaceAndComments(ps);

		Token tok = ps.front();
		if(tok.type != TType::EndOfFile)
		{
			switch(tok.type)
			{
				case TType::Var:
				case TType::Val:
				case TType::Func:
				case TType::Enum:
				case TType::Class:
				case TType::Static:
				case TType::Struct:
				case TType::Public:
				case TType::Private:
				case TType::Operator:
				case TType::Protocol:
				case TType::Internal:
				case TType::Override:
				case TType::Extension:
				case TType::Namespace:
				case TType::TypeAlias:
				case TType::ForeignFunc:
					parserError("Declarations are not expressions (%s)", tok.text.to_string().c_str());

				case TType::Dealloc:
					parserError("Deallocation statements are not expressions");

				case TType::Defer:
					parserError("Defer statements are not expressions");

				case TType::Return:
					parserError("Return statements are not expressions");

				case TType::Break:
					parserError("Break statements are not expressions");

				case TType::Continue:
					parserError("Continue statements are not expressions");

				case TType::If:
					parserError("If statements are not expressions");

				// since both have the same kind of AST node, parseWhile can handle both
				case TType::Do:
				case TType::While:
				case TType::Loop:
					parserError("Loops are not expressions");







				case TType::LParen:
					return parseParenthesised(ps);

				case TType::Identifier:
				case TType::UnicodeSymbol:
					if(tok.text == "init")
						return parseInitFunc(ps);

					return parseIdExpr(ps);

				case TType::Alloc:
					return parseAlloc(ps);

				case TType::Typeof:
					return parseTypeof(ps);

				case TType::StringLiteral:
					return parseStringLiteral(ps);

				case TType::Number:
					return parseNumber(ps);

				case TType::LSquare:
					return parseArrayLiteral(ps);

				case TType::At:
					parseAttribute(ps);
					return parsePrimary(ps);

				// // shit you just skip
				// case TType::NewLine:
				// 	ps.currentPos.line++;
				// 	// fallthrough

				// case TType::Comment:
				// case TType::Semicolon:
				// 	ps.eat();
				// 	return CreateAST(DummyExpr, tok);

				// no point creating separate functions for these
				case TType::True:
					ps.pop();
					return CreateAST(BoolVal, tok, true);

				case TType::False:
					ps.pop();
					return CreateAST(BoolVal, tok, false);

				// nor for this
				case TType::Null:
					ps.pop();
					return CreateAST(NullVal, tok);


				case TType::LBrace:
					// parse it, but throw it away
					parserMessage(Err::Warn, parseBracedBlock(ps)->pin, "Anonymous blocks are ignored; to run, preface with 'do'");
					return CreateAST(DummyExpr, ps.front());

				default:
					parserError(tok, "Unexpected token '%s' (id = %d)", tok.text.to_string().c_str(), tok.type);
			}
		}

		return 0;
	}











	Expr* parseUnary(ParserState& ps)
	{
		Token tk = ps.front();

		// check for unary shit
		ArithmeticOp op = ArithmeticOp::Invalid;

		if(tk.type == TType::Exclamation)		op = ArithmeticOp::LogicalNot;
		else if(tk.type == TType::Plus)			op = ArithmeticOp::Plus;
		else if(tk.type == TType::Minus)		op = ArithmeticOp::Minus;
		else if(tk.type == TType::Tilde)		op = ArithmeticOp::BitwiseNot;
		else if(tk.type == TType::Asterisk)		op = ArithmeticOp::Deref;
		else if(tk.type == TType::Ampersand)	op = ArithmeticOp::AddrOf;

		if(op != ArithmeticOp::Invalid)
		{
			ps.eat();

			int prec = getOpPrecForUnaryOp(op);
			Expr* un = parseUnary(ps);
			Expr* thing = parseRhs(ps, un, prec);

			return CreateAST(UnaryOp, tk, op, thing);
		}

		return parsePrimary(ps);
	}








	Expr* parseStaticDecl(ParserState& ps)
	{
		iceAssert(ps.front().type == TType::Static);
		if(ps.structNestLevel == 0)
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
			parserError("Invaild static expression '%s'", ps.front().text.to_string().c_str());
		}
	}


	// this function expects the first token it sees to be '('
	FuncDecl* parseFuncDeclUsingIdentifierToken(ParserState& ps, Token _id)
	{
		if(_id.type != TType::Identifier && _id.type != TType::UnicodeSymbol)
			parserError("Expected identifier, but got token of type %d", ps.front().type);

		Token func_id = _id;
		std::string id = func_id.text.to_string();

		std::map<std::string, TypeConstraints_t> genericTypes;

		// expect a left bracket
		Token paren = ps.eat();
		if(paren.type != TType::LParen && paren.type != TType::LAngle)
		{
			parserError("Expected '(' in function declaration, got '%s'", paren.text.to_string().c_str());
		}
		else if(paren.type == TType::LAngle)
		{
			if(ps.front().type == TType::RAngle)
				parserError("Empty type parameter list");

			genericTypes = parseGenericTypeList(ps);

			if(ps.eat().type != TType::LParen)
				parserError("Expected '(' after function name");
		}






		bool isCVA = false;
		bool isVariableArg = false;

		// get the parameter list
		// expect an identifer, colon, type
		std::vector<VarDecl*> params;
		std::map<std::string, VarDecl*> nameCheck;

		while(ps.hasTokens() && ps.front().type != TType::RParen)
		{
			Token tok_id;
			if((tok_id = ps.eat()).type != TType::Identifier)
			{
				if(tok_id.type == TType::Ellipsis)
				{
					isCVA = true;
					if(ps.front().type != TType::RParen)
						parserError("Vararg must be last in the function declaration");

					break;
				}
				else
				{
					parserError(tok_id, "Expected identifier (got '%s')", tok_id.text.to_string().c_str());
				}
			}

			std::string id = tok_id.text.to_string();
			VarDecl* v = CreateAST(VarDecl, tok_id, id, true);

			// expect a colon
			if(ps.eat().type != TType::Colon)
				parserError("Expected ':' followed by a type");

			v->ptype = parseType(ps);


			// NOTE(ghetto): FUCKING. GHETTO.
			if(isVariableArg && v->ptype->isVariadicArrayType())
				parserError("Vararg must be last in the function declaration");

			else if(v->ptype->isVariadicArrayType())
				isVariableArg = true;



			if(!nameCheck[v->ident.name])
			{
				params.push_back(v);
				nameCheck[v->ident.name] = v;
			}
			else
			{
				parserError("Redeclared variable '%s' in argument list", v->ident.name.c_str());
			}


			if(ps.front().type == TType::Comma)
				ps.eat();
		}

		// consume the closing paren
		ps.eat();

		// get return type.
		pts::Type* ret = 0;
		Pin retPin;
		if(ps.hasTokens() && ps.front().type == TType::Arrow)
		{
			ps.eat();
			retPin = ps.front().pin;

			ret = parseType(ps);
		}
		else
		{
			ret = pts::NamedType::create(VOID_TYPE_STRING);
		}

		FuncDecl* f = CreateAST(FuncDecl, func_id, id, params, ret);
		f->attribs = checkAndApplyAttributes(ps, Attr_VisPublic | Attr_VisInternal | Attr_VisPrivate |
			Attr_NoMangle | Attr_ForceMangle | Attr_Override);

		f->isCStyleVarArg = isCVA;
		f->isVariadic = isVariableArg;
		f->genericTypes = genericTypes;

		f->returnTypePos = retPin;

		if(f->isCStyleVarArg && f->isVariadic)
			parserError("C-style variadic arguments and Flax-style variadic arguments are mutually exclusive.");

		return f;
	}


	FuncDecl* parseFuncDecl(ParserState& ps)
	{
		// for(size_t i = ps.index; i < ps.tokens.size(); ++i)
		// {
		// 	ps.tokens[i].text.to_string();
		// }

		iceAssert(ps.eat().type == TType::Func);

		auto id = ps.eat();
		if(id.type != TType::Identifier && id.type != TType::UnicodeSymbol)
			parserError("Expected identifier, but got token of type %d", id.type);

		return parseFuncDeclUsingIdentifierToken(ps, id);
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
			std::string lftype = ftype.text.to_string();
			std::transform(lftype.begin(), lftype.end(), lftype.begin(), ::tolower);

			if(lftype == "c")			ffitype = FFIType::C;
			else						parserError("Unknown FFI type '%s'", ftype.text.to_string().c_str());

			if(ps.eat().type != TType::RParen)
				parserError("Expected ')'");
		}



		FuncDecl* decl = parseFuncDecl(ps);
		decl->isFFI = true;
		decl->ffiType = ffitype;

		return CreateAST(ForeignFuncDecl, func, decl);
	}

	BracedBlock* parseBracedBlock(ParserState& ps, bool hadOpeningBrace, bool eatClosingBrace)
	{
		Token tok_brc = ps.eat();
		BracedBlock* c = CreateAST(BracedBlock, tok_brc);

		// make sure the first token is a left brace.
		if(tok_brc.type != TType::LBrace && !hadOpeningBrace)
			parserError("Expected '{' to begin a block, found '%s'!", tok_brc.text.to_string().c_str());

		// get the stuff inside.
		while(ps.hasTokens() && ps.front().type != TType::RBrace)
		{
			// Expr* e = parseExpr(ps);
			Expr* e = parseStatement(ps);
			if(!e) break;

			DeferredExpr* d = 0;

			if((d = dynamic_cast<DeferredExpr*>(e)))
				c->deferredStatements.push_back(d);

			else if(!dynamic_cast<DummyExpr*>(e))
				c->statements.push_back(e);
		}

		if(ps.front().type != TType::RBrace)
			parserError(tok_brc, "Expected '}', found '%s' (id = %d)", ps.front().text.to_string().c_str(), ps.front().type);

		if(eatClosingBrace)
			ps.pop();

		std::reverse(c->deferredStatements.begin(), c->deferredStatements.end());
		return c;
	}

	Func* parseFunc(ParserState& ps)
	{
		iceAssert(ps.eat().type == TType::Func);

		auto id = ps.eat();
		if(id.type != TType::Identifier && id.type != TType::UnicodeSymbol)
			parserError("Expected identifier, but got token of type %d", id.type);

		return parseFuncUsingIdentifierToken(ps, id);
	}


	Func* parseFuncUsingIdentifierToken(ParserState& ps, Token front)
	{
		FuncDecl* decl = parseFuncDeclUsingIdentifierToken(ps, front);

		if(ps.lookaheadUntilNonNewline().type == TType::LBrace)
		{
			ps.skipNewline();
			return CreateAST(Func, front, decl, parseBracedBlock(ps));
		}
		else
		{
			return CreateAST(Func, front, decl, 0);
		}
	}





	Expr* parseInitFunc(ParserState& ps)
	{
		Token front = ps.front();
		iceAssert(front.text == "init");

		// we need to disambiguate between calling the init() function, and defining an init() function
		// to do this, we can loop through the tokens (without consuming) until we find the closing ')'
		// then see if the token following that is a '{'. if so, it's a declaration, if not it's a call

		if(ps.getRemainingTokens() < 3)
			parserError("Unexpected end of input");

		else if(ps.getRemainingTokens() > 3 && ps.lookahead(1).type != TType::LParen)
			parserError("Expected '(' for either function call or declaration");

		int parenLevel = 0;
		bool foundBrace = false;
		for(size_t i = 1; i < ps.getRemainingTokens(); i++)
		{
			if(ps.lookahead(i).type == TType::LParen)
			{
				parenLevel++;
			}
			else if(ps.lookahead(i).type == TType::RParen)
			{
				parenLevel--;
				if(parenLevel == 0)
				{
					// look through each until we find a brace
					for(size_t k = i + 1; k < ps.getRemainingTokens() && !foundBrace; k++)
					{
						if(ps.lookahead(k).type == TType::Comment || ps.lookahead(k).type == TType::NewLine)
							continue;

						else if(ps.lookahead(k).type == TType::LBrace)
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
			// eat the init token.
			ps.eat();

			FuncDecl* decl = parseFuncDeclUsingIdentifierToken(ps, front);
			return CreateAST(Func, front, decl, parseBracedBlock(ps));
		}
		else
		{
			// no brace, it's a call
			// eat the "init" token
			// ps.eat();

			return parseFuncCall(ps, "init", ps.eat().pin);
		}
	}







	static std::string parseStringTypeIndirections(ParserState& ps)
	{
		std::string ret;

		// handle pointers.
		while(ps.hasTokens() && ps.front().type == TType::Asterisk)
		{
			ret += "*";
			ps.eat();
		}


		// handle arrays
		// check if the next token is a '['.
		while(ps.hasTokens() && ps.front().type == TType::LSquare)
		{
			ps.eat();

			// this parses multi-dim array types.
			Token n = ps.eat();
			std::string dims;

			if(n.type == TType::Number)
			{
				dims = "[" + n.text.to_string() + "]";
			}
			else if(n.type == TType::Ellipsis)
			{
				dims = "[...]";
			}
			else if(n.type == TType::RSquare)
			{
				dims = "[]";
			}
			else
			{
				parserError("Expected integer size for fixed-length array, closing ']' for variable-sized array, or ellipsis for "
					"a variadic function argument.");
			}

			bool isVarArray = false;
			if(n.type == TType::Ellipsis)
				isVarArray = true;


			if(dims != "[]")
			{
				n = ps.eat();
				if(n.type != TType::RSquare)
					parserError(n, "Expected ']' in type specifier, have '%s'", n.text.to_string().c_str());
			}

			ret += dims;

			if(isVarArray && ps.front().type == TType::LSquare)
				parserError("Variadic array must be the last (outermost) dimension");
		}

		if(ps.hasTokens() && ps.front().type == TType::Asterisk)
			ret += parseStringTypeIndirections(ps);

		return ret;
	}


	// todo(ugly): this is quite stupid
	// it's basically a token-based duplication of the thing we have in ParserTypeSystem.cpp
	static std::string parseStringType(ParserState& ps)
	{
		// note: use of pop() vs eat() here is to stop eating newlines, that cause identifiers on the next line to be
		// conflated with the current type being parsed.
		if(!ps.hasTokens()) return "";

		Token front = ps.front();

		if(front.type == TType::Identifier)
		{
			std::string ret = front.text.to_string();
			ps.pop();

			while(ps.hasTokens())
			{
				if(ps.front().type == TType::Period)
				{
					if(ret.back() == '.')
						parserError("Extraneous '.' in scoped type specifier");

					ret += ".";
					ps.pop();
				}
				else if(ps.front().type == TType::Identifier && (ret.empty() || ret.back() == '.'))
				{
					ret += ps.front().text.to_string();
					ps.pop();
				}
				else
				{
					break;
				}
			}


			// check to see if we have even more stuff
			// eg. T**[10]*[3]**

			return ret + parseStringTypeIndirections(ps);
		}
		else if(front.type == TType::LParen)
		{
			ps.pop();

			// parse a tuple.
			std::vector<std::string> types;
			while(ps.hasTokens() && ps.front().type != TType::RParen)
			{
				// do things.
				std::string ret = parseStringType(ps);

				if(ps.front().type != TType::Comma && ps.front().type != TType::RParen)
					parserError("Unexpected token '%s' in type specifier, expected either ',' or ')'", ps.front().text.to_string().c_str());

				else if(ps.front().type == TType::Comma)
					ps.eat();


				types.push_back(ret);
			}

			if(types.size() == 0)
				parserError("Empty tuples '()' are not supported");

			std::string ret = "(";
			for(auto t : types)
				ret += t + ",";

			if(ret.size() > 1)
				ret.pop_back();

			ret += ")";


			if(ps.eat().type != TType::RParen)
				parserError("Expected ')' to end tuple type specifier, found '%s' instead", ps.front().text.to_string().c_str());


			// ok, now check if we have more things behind this.
			// eg. (int, int)**[10]*

			return ret + parseStringTypeIndirections(ps);
		}
		else if(front.type == TType::LSquare)
		{
			// function type specifiers have this form:
			// [<T, U> (a: T, b: U) -> X]
			// HOWEVER, on the PTS system, we use {<T, U>(...) -> ...}.
			// basically, [ -> { and ] -> }.
			// this is to disambiguate from the array thing.

			std::string ret = "{";

			ps.pop();
			std::map<std::string, TypeConstraints_t> genericTypes;

			if(ps.hasTokens() && ps.front().type != TType::LAngle && ps.front().type != TType::LParen)
			{
				parserError("Expected '(' to begin argument list of function type specifier, got '%s' instead",
					ps.front().text.to_string().c_str());
			}
			else if(ps.hasTokens() && ps.front().type == TType::LAngle)
			{
				ps.pop();
				genericTypes = parseGenericTypeList(ps);
			}

			if(genericTypes.size() > 0)
			{
				ret += "<";
				for(auto g : genericTypes)
				{
					ret += g.first;
					if(g.second.protocols.size() > 0)
						ret += ":";

					for(auto p : g.second.protocols)
						ret += p + "&";

					if(ret.back() == '&')
						ret.pop_back();

					ret += ",";
				}

				if(ret.back() == ',')
					ret.pop_back();

				if(ret.back() == '<')
					parserError("Empty generic type lists are not supported");

				ret += ">";
			}


			// ok, add the actual function shit now.
			ret += "(";

			if(ps.front().type != TType::LParen)
			{
				parserError("Expected '(' to begin argument list of function type specifier, got '%s' instead",
					ps.front().text.to_string().c_str());
			}

			ps.pop();

			// start. basically we take a list of types only, no names.
			while(ps.hasTokens() && ps.front().type != TType::RParen)
			{
				ret += parseStringType(ps);

				if(ps.front().type == TType::Comma)
				{
					ps.eat();
					ret += ",";
				}
				else if(ps.front().type != TType::RParen)
				{
					parserError("Expected ')' to end argument list of function type specifier, got '%s' instead",
						ps.front().text.to_string().c_str());
				}
			}

			if(ret.back() == ',')
				ret.pop_back();

			iceAssert(ps.eat().type == TType::RParen);
			ret += ")";

			if(ps.front().type != TType::Arrow)
			{
				parserMessage(Err::Error, "Expected '->' to specify return type of function type specifier, got '%s'",
					ps.front().text.to_string().c_str());

				parserMessage(Err::Info, "Note: void returns must be made explicit in type specifiers");

				doTheExit();
			}

			ps.pop();

			ret += "->" + parseStringType(ps) + "}";

			if(ps.front().type != TType::RSquare)
				parserError("Expected ']' to end function type specifier, got '%s' instead", ps.front().text.to_string().c_str());

			ps.pop();


			// see if we have... more.
			return ret + parseStringTypeIndirections(ps);
		}
		else
		{
			parserError("Expected type specifier, found invalid token '%s' instead", front.text.to_string().c_str());
		}
	}




	pts::Type* parseType(ParserState& ps)
	{
		return pts::parseType(parseStringType(ps));
	}




	static ComputedProperty* parseComputedProperty(ParserState& ps, std::string name, pts::Type* type, uint64_t attribs, Token tok_id)
	{
		if(ps.structNestLevel == 0)
			parserError("Computed properties can only be declared inside classes");

		// computed property, getting and setting

		// eat the brace, skip whitespace
		ComputedProperty* cprop = CreateAST(ComputedProperty, tok_id, name);

		iceAssert(ps.eat().type == TType::LBrace);

		cprop->ptype = type;
		cprop->attribs = attribs;

		bool didGetter = false;
		bool didSetter = false;
		for(int i = 0; i < 2; i++)
		{
			if(ps.lookaheadUntilNonNewline().type == TType::Get)
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
			else if(ps.lookaheadUntilNonNewline().type == TType::Set)
			{
				if(didSetter)
					parserError("Only one setter is allowed per computed property");

				didSetter = true;

				ps.eat();
				std::string setValName = "newValue";

				// see if we have parentheses
				if(ps.front().type == TType::LParen)
				{
					ps.eat();
					if(ps.front().type != TType::Identifier)
						parserError("Expected identifier for custom setter argument name");

					setValName = ps.eat().text.to_string();

					if(ps.eat().type != TType::RParen)
						parserError("Expected closing ')'");
				}

				cprop->setter = parseBracedBlock(ps);
				cprop->setterArgName = setValName;
			}
			else if(ps.lookaheadUntilNonNewline().type == TType::RBrace)
			{
				ps.skipNewline();
				break;
			}
			else
			{
				// implicit read-only, 'get' not required
				// there's no set, so make i = 1 so we error on extra bits
				i = 1;

				// didHaveOpeningBrace -- true
				// eatClosingBrace -- false
				cprop->getter = parseBracedBlock(ps, true, false);

				didGetter = true;
			}
		}

		if(!didGetter)
			parserError(tok_id, "Computed properties must have at least a getter.");

		if(ps.eat().type != TType::RBrace)
			parserError("Expected closing '}'");

		return cprop;
	}


	VarDecl* parseVarDecl(ParserState& ps)
	{
		iceAssert(ps.front().type == TType::Var || ps.front().type == TType::Val);

		bool immutable = (ps.front().type == TType::Val);
		uint64_t attribs = checkAndApplyAttributes(ps, Attr_NoAutoInit | Attr_VisPublic | Attr_VisInternal | Attr_VisPrivate | Attr_Override);

		ps.eat();

		// get the identifier.
		Token tok_id;
		if((tok_id = ps.eat()).type != TType::Identifier)
			parserError("Expected identifier for variable declaration.");

		std::string id = tok_id.text.to_string();
		VarDecl* v = CreateAST(VarDecl, tok_id, id, immutable);
		v->disableAutoInit = attribs & Attr_NoAutoInit;
		v->attribs = attribs;

		// check the type.
		bool isInferred = false;
		Token colon = ps.eat();
		if(colon.type == TType::Colon)
		{
			v->ptype = parseType(ps);

			if(ps.lookaheadUntilNonNewline().type == TType::LBrace)
			{
				ps.skipNewline();
				return parseComputedProperty(ps, v->ident.name, v->ptype, v->attribs, tok_id);
			}
		}
		else if(colon.type == TType::Equal)
		{
			v->ptype = pts::InferredType::get();
			isInferred = true;
		}
		else
		{
			parserError("Variable declaration without type requires initialiser for type inference");
		}


		if(ps.front().type == TType::Equal || isInferred)
		{
			if(!isInferred) ps.eat();

			v->initVal = parseExpr(ps);
		}
		else if(immutable)
		{
			parserError("Constant variables require an initialiser at the declaration site");
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

		// ps.leftParenNestLevel--;
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
		// ps.leftParenNestLevel++;

		return CreateAST(Tuple, first, values);
	}

	Expr* parseParenthesised(ParserState& ps)
	{
		iceAssert(ps.eat().type == TType::LParen);
		ps.leftParenNestLevel++;


		Expr* within = parseExpr(ps);

		// if we're a tuple, get ready for this shit.
		if(ps.front().type == TType::Comma)
		{
			// remove the comma
			ps.eat();

			// parse a tuple
			Expr* tup = parseTuple(ps, within);
			within = tup;
		}

		iceAssert(ps.front().type == TType::RParen);
		ps.eat();

		ps.leftParenNestLevel--;
		return within;
	}

	Expr* parseExpr(ParserState& ps)
	{
		Expr* lhs = parseUnary(ps);
		iceAssert(lhs);

		return parseRhs(ps, lhs, 0);
	}

	static Expr* parsePostfixUnaryOp(ParserState& ps, Token tok, Expr* curLhs)
	{
		// do something! quickly!

		// get the type of op.
		// prec: array index: 120

		Token top = tok;
		Expr* newlhs = 0;
		if(top.type == TType::LSquare)
		{
			// parse the inside expression
			Expr* inside = parseExpr(ps);
			if(ps.eat().type != TType::RSquare)
				parserError("Expected ']' after '[' for array index");

			newlhs = CreateAST_Pin(ArrayIndex, curLhs->pin, curLhs, inside);
		}
		else
		{
			// todo: ++ and --.
			parserError("enotsup");
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
						// < < is <<
						op = ArithmeticOp::ShiftLeft;
						ps.eat();
					}
					else if(next1.type == TType::LessThanEquals)
					{
						// < <= is <<=
						op = ArithmeticOp::ShiftLeftEquals;
						ps.eat();
					}
				}
				else if(tok_op.type == TType::RAngle)
				{
					if(next1.type == TType::RAngle)
					{
						// > > is >>
						op = ArithmeticOp::ShiftRight;
						ps.eat();
					}
					else if(next1.type == TType::GreaterEquals)
					{
						// > >= is >>=
						op = ArithmeticOp::ShiftRightEquals;
						ps.eat();
					}
				}
			}
			// else if(tok_op.type == TType::Comma && ps.leftParenNestLevel > 0)
			// {
			// 	// return parseTuple(ps, lhs), ps.leftParenNestLevel--;
			// 	auto ret = parseTuple(ps, lhs);
			// 	// ps.leftParenNestLevel--;

			// 	return ret;
			// }
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
					case TType::Caret:			op = ArithmeticOp::BitwiseXor;			break;
					case TType::LogicalOr:		op = ArithmeticOp::LogicalOr;			break;
					case TType::LogicalAnd:		op = ArithmeticOp::LogicalAnd;			break;

					case TType::PlusEq:			op = ArithmeticOp::PlusEquals;			break;
					case TType::MinusEq:		op = ArithmeticOp::MinusEquals;			break;
					case TType::MultiplyEq:		op = ArithmeticOp::MultiplyEquals;		break;
					case TType::DivideEq:		op = ArithmeticOp::DivideEquals;		break;
					case TType::ModEq:			op = ArithmeticOp::ModEquals;			break;
					case TType::ShiftLeftEq:	op = ArithmeticOp::ShiftLeftEquals;		break;
					case TType::ShiftRightEq:	op = ArithmeticOp::ShiftRightEquals;	break;
					case TType::AmpersandEq:	op = ArithmeticOp::BitwiseAndEquals;	break;
					case TType::PipeEq:			op = ArithmeticOp::BitwiseOrEquals;		break;
					case TType::CaretEq:		op = ArithmeticOp::BitwiseXorEquals;	break;

					case TType::Period:			op = ArithmeticOp::MemberAccess;		break;
					case TType::As:				op = (tok_op.text == "as!") ? ArithmeticOp::ForcedCast : ArithmeticOp::Cast;
												break;
					default:
					{
						if(ps.cgi->customOperatorMapRev.find(tok_op.text.to_string()) != ps.cgi->customOperatorMapRev.end())
						{
							op = ps.cgi->customOperatorMapRev[tok_op.text.to_string()];
							break;
						}
						else
						{
							parserError("Unknown operator '%s'", tok_op.text.to_string().c_str());
						}
					}
				}
			}














			Expr* rhs = 0;
			if(tok_op.type == TType::As)
			{
				rhs = CreateAST(DummyExpr, tok_op);
				rhs->ptype = parseType(ps);
			}
			else
			{
				rhs = parseUnary(ps);
			}

			int next = getCurOpPrec(ps);

			if(next > prec || isRightAssociativeOp(ps.front()))
				rhs = parseRhs(ps, rhs, prec + 1);

			// todo: chained relational operators
			// eg. 1 == 1 < 4 > 3 > -5 == -7 + 2 < 10 > 3

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
		std::string id = tok_id.text.to_string();
		VarRef* idvr = CreateAST(VarRef, tok_id, id);

		if(ps.front().type == TType::LParen)
		{
			auto ret = parseFuncCall(ps, id, idvr->pin);
			delete idvr;

			return ret;
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

		Alloc* ret = CreateAST(Alloc, tok_alloc);


		// todo:
		// check for comma, to allocate arrays on the heap
		// ie. let arr = alloc [1, 2, 3].
		// obviously, type is not necessary.
		// probably. if we need to (for polymorphism, to specify the base type, for example)
		// then either
		// alloc: Type [1, 2, 3] or alloc [1, 2, 3]: Type will work.
		// not too hard to implement either.


		// right now we handle multi dim in the form of
		// alloc[x][y][z] Type(params)
		while(ps.front().type == TType::LSquare)
		{
			ps.eat();

			// this parses multi-dim array types.
			ret->counts.push_back(parseExpr(ps));

			Token n = ps.eat();
			if(n.type != TType::RSquare)
				parserError("Expected ']', have %s", n.text.to_string().c_str());
		}

		auto pin = ps.front().pin;
		pts::Type* type = parseType(ps);

		if(ps.front().type == TType::LParen)
		{
			// alloc[...] Foo(...)
			if(!type->isNamedType())
				parserError("Only class or struct types can be used in an alloc-construct expression");

			FuncCall* fc = parseFuncCall(ps, type->str(), pin);
			ret->params = fc->params;
		}

		ret->ptype = type;

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
		iceAssert(ps.front().type == TType::Number);
		auto t = ps.eat();

		return CreateAST(Number, t, t.text.to_string());


		#if 0
		Number* n;
		if(ps.front().type == TType::Integer)
		{
			// todo: handle integer suffixes

			Token tok = ps.eat();
			auto iv = getIntegerValue(tok);

			n = CreateAST(Number, tok, iv.first);
			if(iv.second)
			{
				n->needUnsigned = true;
				n->ptype = pts::NamedType::create(UINT64_TYPE_STRING);
			}
			else
			{
				n->ptype = pts::NamedType::create(INT64_TYPE_STRING);
			}
		}
		else if(ps.front().type == TType::Decimal)
		{
			Token tok = ps.eat();
			n = CreateAST(Number, tok, getDecimalValue(tok));

			if(n->dval < (double) FLT_MAX)	n->ptype = pts::NamedType::create(FLOAT32_TYPE_STRING);
			else							n->ptype = pts::NamedType::create(FLOAT64_TYPE_STRING);
		}
		else
		{
			parserError("What!????");
			iceAssert(false);
			return 0;
		}

		return n;
		#endif
	}

	FuncCall* parseFuncCall(ParserState& ps, std::string id, Pin id_pos)
	{
		Token tk = ps.eat();
		iceAssert(tk.type == TType::LParen);

		std::vector<Expr*> args;

		int save = ps.leftParenNestLevel;
		ps.leftParenNestLevel = 0;


		if(ps.front().type != TType::RParen)
		{
			while(true)
			{
				Expr* arg = parseExpr(ps);

				if(arg == 0)
					return 0;


				args.push_back(arg);
				if(ps.front().type == TType::RParen)
				{
					ps.eat();
					break;
				}

				Token t;
				if((t = ps.eat()).type != TType::Comma)
					parserError("Expected either ',' or ')' in parameter list, got '%s' (id = %s)", t.text.to_string().c_str(), id.c_str());
			}
		}
		else
		{
			ps.eat();
		}

		ps.leftParenNestLevel = save;

		auto ret = CreateAST_Pin(FuncCall, id_pos, id, args);

		return ret;
	}

	Return* parseReturn(ParserState& ps)
	{
		Token front = ps.eat();
		iceAssert(front.type == TType::Return);

		Expr* retval = 0;

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
		std::vector<CCPair> conds;

		Expr* cond = parseExpr(ps);
		BracedBlock* tcase = parseBracedBlock(ps);

		conds.push_back(CCPair(cond, tcase));

		// check for else and else if
		BracedBlock* ecase = 0;
		bool parsedElse = false;
		while(ps.lookaheadUntilNonNewline().type == TType::Else)
		{
			ps.eat();
			if(ps.lookaheadUntilNonNewline().type == TType::If && !parsedElse)
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
		Token tok_front = ps.eat();

		if(tok_front.type == TType::While)
		{
			if(ps.lookaheadUntilNonNewline().type == TType::LBrace)
				parserError(tok_front, "Expected condition expression after 'while'");

			Expr* cond = parseExpr(ps);
			BracedBlock* body = parseBracedBlock(ps);

			return CreateAST(WhileLoop, tok_front, cond, body, false);
		}
		else
		{
			iceAssert(tok_front.type == TType::Do || tok_front.type == TType::Loop);

			// parse the block first
			BracedBlock* body = parseBracedBlock(ps);

			// syntax treat: since a raw block is ignored (for good reason, how can we reference it?)
			// we can use 'do' to run an anonymous block
			// therefore, the 'while' clause at the end is optional; if it's not present, then the condition is false.

			// 'loop' and 'do', when used without the 'while' clause on the end, have opposite behaviours
			// do { ... } runs the block only once, while loop { ... } runs it infinite times.
			// with the 'while' clause, they have the same behaviour.

			Expr* cond = 0;

			// save the ps position
			size_t savedPosition = ps.index;

			if(ps.lookaheadUntilNonNewline().type == TType::While)
			{
				// here's the thing -- something like this:
				// do { ... } while { ... }
				// should really parse as 'do {} while' '{ }'
				// this then causes an error (expected condition expr after while), and an unnamed block.

				ps.eat();

				// we need the condition either way
				if(ps.lookaheadUntilNonNewline().type == TType::LBrace)
					parserError(tok_front, "Expected condition expression after 'while'");

				cond = parseExpr(ps);

				// ok, now check if we have a { } after the while
				if(ps.lookaheadUntilNonNewline().type == TType::LBrace)
				{
					// oh kurwa, what actually was written was do { ... } ;;; while(cond) { ... }

					// set the cond first,
					cond = CreateAST(BoolVal, ps.front(), false);

					// then, we restore the state; we've parsed the while and the expr already.
					// but we saved the index, so we just hand over the receipt and ask for a refund.

					ps.refundTokens(ps.index - savedPosition);
				}
			}
			else
			{
				// here's the magic: continue condition is 'false' for do, 'true' for loop
				cond = CreateAST(BoolVal, ps.front(), tok_front.type == TType::Do ? false : true);
			}

			return CreateAST(WhileLoop, tok_front, cond, body, true);
		}
	}


	static std::vector<std::string> parseInheritanceList(ParserState& ps, std::string selfname)
	{
		std::vector<std::string> ret;
		while(true)
		{
			Token id = ps.eat();
			if(id.type != TType::Identifier)
				parserError("Expected identifier after ':' in struct or class declaration");

			if(std::find(ret.begin(), ret.end(), id.text) != ret.end())
				parserError("Duplicate member %s in inheritance list", id.text.to_string().c_str());

			if(selfname == id.text)
				parserError("Self inheritance is illegal");

			ret.push_back(id.text.to_string());

			if(ps.lookaheadUntilNonNewline().type != TType::Comma)
				break;

			ps.eat();
		}

		return ret;
	}

	static std::map<std::string, TypeConstraints_t> parseGenericTypeList(ParserState& ps)
	{
		std::map<std::string, TypeConstraints_t> ret;

		while(ps.front().type != TType::RAngle)
		{
			if(ps.front().type == TType::Identifier)
			{
				std::string gt = ps.eat().text.to_string();
				TypeConstraints_t constrs;

				if(ps.front().type == TType::Colon)
				{
					ps.eat();
					if(ps.front().type != TType::Identifier)
						parserError("Expected identifier after beginning of type constraint list");

					while(ps.front().type == TType::Identifier)
					{
						constrs.protocols.push_back(ps.eat().text.to_string());

						if(ps.front().type == TType::Ampersand)
						{
							ps.eat();
						}
						else if(ps.front().type != TType::Comma && ps.front().type != TType::RAngle)
						{
							parserError("Expected ',' or '>' to end type parameter list (1)");
						}
					}
				}
				else if(ps.front().type != TType::Comma && ps.front().type != TType::RAngle)
				{
					parserError("Expected ',' or '>' to end type parameter list (2)");
				}

				ret[gt] = constrs;
			}
			else if(ps.front().type == TType::Comma)
			{
				ps.eat();
			}
			else if(ps.front().type != TType::RAngle)
			{
				parserError("Expected '>' to end type parameter list");
			}
		}

		iceAssert(ps.eat().type == TType::RAngle);

		return ret;
	}






	StructDef* parseStruct(ParserState& ps)
	{
		Token tok_str = ps.eat();
		iceAssert(tok_str.type == TType::Struct);

		ps.structNestLevel++;
		Token tok_id = ps.eat();

		if(tok_id.type != TType::Identifier)
			parserError("Expected identifier");

		std::string id = tok_id.text.to_string();
		StructDef* str = CreateAST(StructDef, tok_id, id);

		uint64_t attr = checkAndApplyAttributes(ps, Attr_PackedStruct | Attr_VisPublic | Attr_VisInternal | Attr_VisPrivate);
		if(attr & Attr_PackedStruct)
			str->packed = true;

		str->attribs = attr;


		// parse a block.
		BracedBlock* body = parseBracedBlock(ps);
		std::unordered_map<std::string, VarDecl*> nameMap;

		for(Expr* stmt : body->statements)
		{
			if(ComputedProperty* cprop = dynamic_cast<ComputedProperty*>(stmt))
			{
				parserError(cprop->pin, "Structs cannot contain properties");
			}
			if(VarDecl* var = dynamic_cast<VarDecl*>(stmt))
			{
				if(nameMap.find(var->ident.name) != nameMap.end())
				{
					parserMessage(Err::Error, var->pin, "Duplicate member: %s", var->ident.name.c_str());
					parserMessage(Err::Info, nameMap[var->ident.name]->pin, "Previous declaration was here.");
					doTheExit();
				}

				if(var->isStatic)
					parserError(var->pin, "Structs cannot contain static members");

				str->members.push_back(var);

				// don't take up space in the struct if it's static.
				if(!var->isStatic)
				{
					nameMap[var->ident.name] = var;
				}
				else
				{
					// nothing
				}
			}
			else if(Func* fn = dynamic_cast<Func*>(stmt))
			{
				parserError(fn->pin, "Structs cannot contain functions");
			}
			else if(dynamic_cast<AssignOpOverload*>(stmt) || dynamic_cast<SubscriptOpOverload*>(stmt))
			{
				parserError(stmt->pin, "Structs cannot contain operator overloads");
			}
			else
			{
				parserError(stmt->pin, "Found invalid expression type in struct");
			}
		}

		ps.structNestLevel--;
		delete body;
		return str;
	}





	ClassDef* parseClass(ParserState& ps)
	{
		Token tok_cls = ps.eat();
		iceAssert(tok_cls.type == TType::Class);

		ps.structNestLevel++;
		Token tok_id = ps.eat();

		if(tok_id.type != TType::Identifier)
			parserError("Expected identifier (got %s)", tok_id.text.to_string().c_str());

		std::string id = tok_id.text.to_string();
		ClassDef* cls = CreateAST(ClassDef, tok_id, id);

		uint64_t attr = checkAndApplyAttributes(ps, Attr_VisPublic | Attr_VisInternal | Attr_VisPrivate);
		cls->attribs = attr;

		// check for a colon.
		if(ps.lookaheadUntilNonNewline().type == TType::Colon)
		{
			ps.eat();
			cls->protocolstrs = parseInheritanceList(ps, cls->ident.name);
		}


		// parse a block.
		BracedBlock* body = parseBracedBlock(ps);
		std::unordered_map<std::string, VarDecl*> nameMap;

		for(Expr* stmt : body->statements)
		{
			if(ComputedProperty* cprop = dynamic_cast<ComputedProperty*>(stmt))
			{
				cls->cprops.push_back(cprop);
			}
			else if(VarDecl* var = dynamic_cast<VarDecl*>(stmt))
			{
				if(nameMap.find(var->ident.name) != nameMap.end())
				{
					parserMessage(Err::Error, var->pin, "Duplicate member: %s", var->ident.name.c_str());
					parserMessage(Err::Info, nameMap[var->ident.name]->pin, "Previous declaration was here.");
					doTheExit();
				}

				cls->members.push_back(var);

				if(!var->isStatic)
				{
					nameMap[var->ident.name] = var;
				}
			}
			else if(Func* func = dynamic_cast<Func*>(stmt))
			{
				cls->funcs.push_back(func);
			}
			else if(StructBase* sb = dynamic_cast<StructBase*>(stmt))
			{
				cls->nestedTypes.push_back({ sb, 0 });
			}
			else if(AssignOpOverload* aoo = dynamic_cast<AssignOpOverload*>(stmt))
			{
				cls->assignmentOverloads.push_back(aoo);
			}
			else if(SubscriptOpOverload* soo = dynamic_cast<SubscriptOpOverload*>(stmt))
			{
				cls->subscriptOverloads.push_back(soo);
			}
			else if(OpOverload* oo = dynamic_cast<OpOverload*>(stmt))
			{
				cls->operatorOverloads.push_back(oo);
			}
			else if(dynamic_cast<DummyExpr*>(stmt))
			{
				continue;
			}
			else
			{
				parserError(stmt->pin, "Found invalid expression type in class");
			}
		}

		ps.structNestLevel--;
		delete body;
		return cls;
	}

	ExtensionDef* parseExtension(ParserState& ps)
	{
		Token tok_str = ps.eat();
		iceAssert(tok_str.type == TType::Extension);

		ps.structNestLevel++;
		Token tok_id = ps.eat();

		if(tok_id.type != TType::Identifier)
			parserError("Expected identifier");

		std::string id = tok_id.text.to_string();
		ExtensionDef* ext = CreateAST(ExtensionDef, tok_id, id);

		uint64_t attr = checkAndApplyAttributes(ps, Attr_VisPublic | Attr_VisInternal | Attr_VisPrivate);

		ext->attribs = attr;

		// check for a colon.
		if(ps.lookaheadUntilNonNewline().type == TType::Colon)
		{
			ps.eat();
			ext->protocolstrs = parseInheritanceList(ps, ext->ident.name);
		}



		// parse a block.
		BracedBlock* body = parseBracedBlock(ps);
		for(Expr* stmt : body->statements)
		{
			if(ComputedProperty* cprop = dynamic_cast<ComputedProperty*>(stmt))
			{
				ext->cprops.push_back(cprop);
			}
			else if(VarDecl* var = dynamic_cast<VarDecl*>(stmt))
			{
				parserError(var->pin, "Extensions cannot delcare new instance members");
			}
			else if(Func* fn = dynamic_cast<Func*>(stmt))
			{
				ext->funcs.push_back(fn);
			}
			else if(AssignOpOverload* aoo = dynamic_cast<AssignOpOverload*>(stmt))
			{
				ext->assignmentOverloads.push_back(aoo);
			}
			else if(SubscriptOpOverload* soo = dynamic_cast<SubscriptOpOverload*>(stmt))
			{
				ext->subscriptOverloads.push_back(soo);
			}
			else if(OpOverload* oo = dynamic_cast<OpOverload*>(stmt))
			{
				ext->operatorOverloads.push_back(oo);
			}
			else if(StructBase* sb = dynamic_cast<StructBase*>(stmt))
			{
				ext->nestedTypes.push_back({ sb, 0 });
			}
			else
			{
				parserError("Found invalid expression type in struct");
			}
		}

		ps.structNestLevel--;
		delete body;
		return ext;
	}







	ProtocolDef* parseProtocol(ParserState& ps)
	{
		Token tok_cls = ps.eat();
		iceAssert(tok_cls.type == TType::Protocol);

		ps.structNestLevel++;
		Token tok_id = ps.eat();

		if(tok_id.type != TType::Identifier)
			parserError("Expected identifier (got %s)", tok_id.text.to_string().c_str());

		std::string id = tok_id.text.to_string();
		ProtocolDef* prot = CreateAST(ProtocolDef, tok_id, id);

		uint64_t attr = checkAndApplyAttributes(ps, Attr_VisPublic | Attr_VisInternal | Attr_VisPrivate);
		prot->attribs = attr;

		// check for a colon.
		if(ps.lookaheadUntilNonNewline().type == TType::Colon)
		{
			ps.eat();
			prot->protocolstrs = parseInheritanceList(ps, prot->ident.name);
		}






		// parse a block.
		BracedBlock* body = parseBracedBlock(ps);
		std::unordered_map<std::string, VarDecl*> nameMap;

		for(Expr* stmt : body->statements)
		{
			if(ComputedProperty* cprop = dynamic_cast<ComputedProperty*>(stmt))
			{
				parserError(cprop->pin, "Protocols cannot contain properties (yet?)");
			}
			else if(VarDecl* var = dynamic_cast<VarDecl*>(stmt))
			{
				parserError(var->pin, "Protocols cannot contain properties (yet?)");
			}
			else if(Func* func = dynamic_cast<Func*>(stmt))
			{
				prot->funcs.push_back(func);
			}
			else if(AssignOpOverload* aoo = dynamic_cast<AssignOpOverload*>(stmt))
			{
				prot->assignmentOverloads.push_back(aoo);
			}
			else if(SubscriptOpOverload* soo = dynamic_cast<SubscriptOpOverload*>(stmt))
			{
				prot->subscriptOverloads.push_back(soo);
			}
			else if(OpOverload* oo = dynamic_cast<OpOverload*>(stmt))
			{
				prot->operatorOverloads.push_back(oo);
			}
			else if(dynamic_cast<DummyExpr*>(stmt))
			{
				continue;
			}
			else
			{
				parserError(stmt->pin, "Found invalid expression type in class");
			}
		}

		ps.structNestLevel--;
		delete body;
		return prot;
	}







	EnumDef* parseEnum(ParserState& ps)
	{
		iceAssert(ps.eat().type == TType::Enum);

		Token tok_id;
		pts::Type* explicitType = 0;

		if((tok_id = ps.eat()).type != TType::Identifier)
		{
			parserError("Expected identifier after 'enum'");
		}
		if(ps.front().type == TType::Colon)
		{
			// parse an explicit type
			ps.eat();
			explicitType = parseType(ps);
		}


		if(ps.eat().type != TType::LBrace)
			parserError("Expected body after 'enum'");

		if(ps.front().type == TType::RBrace)
			parserError("Empty enumerations are not allowed");


		EnumDef* enumer = CreateAST(EnumDef, tok_id, tok_id.text.to_string());
		enumer->ptype = explicitType;
		Token front = ps.front();

		uint64_t attr = checkAndApplyAttributes(ps, Attr_VisPublic | Attr_VisInternal | Attr_VisPrivate);
		enumer->attribs = attr;

		// parse the stuff.
		bool isFirst = true;
		bool isNumeric = false;
		Number* prevNumber = 0;

		ps.skipNewline();
		while(front = ps.front(), ps.hasTokens() > 0)
		{
			if(front.type == TType::RBrace && !isFirst)
				break;

			if(front.type != TType::Case)
				parserError("Only 'case' expressions are allowed inside enumerations, got '%s'", front.text.to_string().c_str());

			ps.eat();
			if((front = ps.eat()).type != TType::Identifier)
				parserError("Expected identifier after 'case', got '%s'", front.text.to_string().c_str());

			std::string eName = front.text.to_string();
			Expr* value = 0;

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
						val = std::stoll(prevNumber->str) + 1;

					// increment it.
					prevNumber = CreateAST(Number, front, std::to_string(val));
					value = prevNumber;
				}
				else if(isFirst)
				{
					prevNumber = CreateAST(Number, front, "0");

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
				parserError("Unexpected token '%s'", front.text.to_string().c_str());
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

		if(id.type != TType::Identifier && id.type != TType::Operator)
			parserError("Expected attribute name after '@'");

		uint64_t attr = 0;
		if(id.text == ATTR_STR_NOMANGLE)			attr |= Attr_NoMangle;
		else if(id.text == ATTR_STR_FORCEMANGLE)	attr |= Attr_ForceMangle;
		else if(id.text == ATTR_STR_NOAUTOINIT)		attr |= Attr_NoAutoInit;
		else if(id.text == ATTR_STR_PACKEDSTRUCT)	attr |= Attr_PackedStruct;
		else if(id.text == ATTR_STR_STRONG)			attr |= Attr_StrongTypeAlias;
		else if(id.text == ATTR_STR_RAW)			attr |= Attr_RawString;
		else if(id.text == "operator")
		{
			// all handled.
			iceAssert(ps.eat().type == TType::LSquare);

			if(ps.front().type == TType::Number)
			{
				ps.pop();
				if(ps.front().type == TType::Comma)
				{
					ps.eat();
					iceAssert(ps.front().type == TType::Identifier);

					if(ps.front().text == "Commutative")
					{
						attr |= Attr_CommutativeOp;
					}
					else if(ps.front().text == "NotCommutative")
					{
						attr &= ~Attr_CommutativeOp;
					}
					else
					{
						parserError("Expected either 'Commutative' or 'NotCommutative' in @operator, got '%s'",
							ps.front().text.to_string().c_str());
					}

					ps.pop();
				}
				else if(ps.front().type == TType::RSquare)
				{
					// do nothing
				}
			}
			else if(ps.front().type == TType::Identifier)
			{
				if(ps.front().text == "Commutative")
				{
					attr |= Attr_CommutativeOp;
				}
				else if(ps.front().text == "NotCommutative")
				{
					attr &= ~Attr_CommutativeOp;
				}
				else
				{
					parserError("Expected either 'Commutative' or 'NotCommutative' in @operator, got '%s'",
						ps.front().text.to_string().c_str());
				}

				ps.pop();


				if(ps.front().type == TType::Comma)
				{
					ps.eat();
					if(ps.front().type != TType::Number)
						parserError("Expected integer precedence in @operator");

					ps.eat();
				}
			}

			iceAssert(ps.eat().type == TType::RSquare);
		}
		else
		{
			parserError("Unknown attribute '%s'", id.text.to_string().c_str());
		}

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
		Token front;
		iceAssert((front = ps.eat()).type == TType::Import);

		std::string s;
		if(ps.front().type == TType::StringLiteral)
		{
			// todo: fix
			// take it as-is.

			s = ps.front().text.to_string();
		}
		else
		{
			if(ps.front().type != TType::Identifier)
				parserError("Expected identifier after import");

			Token t;
			while(ps.hasTokens() && (t = ps.front()).type != TType::Invalid)
			{
				if(t.type == TType::Period)
					s += ".";

				else if(t.type == TType::Identifier)
					s += t.text.to_string();

				else if(t.type == TType::Asterisk)
					s += "*";

				else
					break;

				ps.pop();
			}
		}

		// NOTE: make sure printAst doesn't touch 'cgi', because this will break to hell.
		return CreateAST(Import, front, s);
	}

	StringLiteral* parseStringLiteral(ParserState& ps)
	{
		iceAssert(ps.front().type == TType::StringLiteral);
		Token tok = ps.eat();

		// do replacement here, instead of in the lexer.
		std::string tmp = tok.text.to_string();
		std::stringstream ss;

		for(size_t i = 0; i < tmp.length(); i++)
		{
			if(tmp[i] == '\\')
			{
				i++;
				switch(tmp[i])
				{
					// todo: handle hex sequences and stuff
					case 'n':	ss << '\n';	break;
					case 'b':	ss << '\b';	break;
					case 'r':	ss << '\r';	break;
					case 't':	ss << '\t';	break;
					case '"':	ss << '\"'; break;
					case '\\':	ss << '\\'; break;
				}

				continue;
			}

			ss << tmp[i];
		}

		auto ret = CreateAST(StringLiteral, tok, ss.str());

		uint64_t attr = checkAndApplyAttributes(ps, Attr_RawString);
		if(attr & Attr_RawString)
			ret->isRaw = true;

		ret->pin.col--;
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

		pts::Type* pt = parseType(ps);

		auto ret = CreateAST(TypeAlias, tok_name, tok_name.text.to_string(), pt);

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
		Token t;
		iceAssert((t = ps.eat()).type == TType::Typeof);

		// require parens
		if(ps.eat().type != TType::LParen)
			parserError("typeof() requires parentheses");

		Expr* inside = parseExpr(ps);

		if(ps.eat().type != TType::RParen)
			parserError("Expected closing ')'");

		return CreateAST(Typeof, t, inside);
	}

	ArrayLiteral* parseArrayLiteral(ParserState& ps)
	{
		iceAssert(ps.front().type == TType::LSquare);
		Token front = ps.eat();

		std::vector<Expr*> values;
		while(true)
		{
			Token tok = ps.lookaheadUntilNonNewline();
			if(tok.type == TType::Comma)
			{
				ps.pop();
				continue;
			}
			else if(tok.type == TType::RSquare)
			{
				break;
			}
			else
			{
				ps.skipNewline();
				values.push_back(parseExpr(ps));
			}
		}

		ps.skipNewline();
		iceAssert(ps.front().type == TType::RSquare);
		ps.eat();

		return CreateAST(ArrayLiteral, front, values);
	}

	NamespaceDecl* parseNamespace(ParserState& ps)
	{
		// todo: investigate if this is the best way.
		// this applies the visibility modifier for the namespace to all statements within.
		// (1 level deep only)
		uint64_t attr = checkAndApplyAttributes(ps, Attr_VisPublic | Attr_VisInternal | Attr_VisPrivate);


		iceAssert(ps.eat().type == TType::Namespace);
		Token tok_id = ps.eat();

		// todo: handle "namespace Foo.Bar.Baz { }", which c++ technically still doesn't have
		// (still a c++1z thing)
		if(tok_id.type != TType::Identifier)
			parserError("Expected identifier after namespace declaration");

		BracedBlock* bb = parseBracedBlock(ps);
		NamespaceDecl* ns = CreateAST(NamespaceDecl, tok_id, tok_id.text.to_string(), bb);


		// do the thing.
		for(auto stmt : bb->statements)
		{
			// make sure the statement doesn't already have its own specifier.
			if(!(stmt->attribs & (Attr_VisPublic | Attr_VisInternal | Attr_VisPrivate)))
				stmt->attribs |= attr;
		}
		for(auto stmt : bb->deferredStatements)
		{
			// make sure the statement doesn't already have its own specifier.
			if(!(stmt->attribs & (Attr_VisPublic | Attr_VisInternal | Attr_VisPrivate)))
				stmt->attribs |= attr;
		}

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
		else if(op == "ix") return ArithmeticOp::Subscript;
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
			case ArithmeticOp::Subscript:			return "ix";
			default:								return cgi->customOperatorMap[op].first;
		}
	}


	Expr* parseOpOverload(ParserState& ps)
	{
		iceAssert(ps.eat().text == "operator");
		Token op = ps.eat();

		ArithmeticOp ao;

		if(op.type == TType::EqualsTo)			ao = ArithmeticOp::CmpEq;
		else if(op.type == TType::NotEquals)	ao = ArithmeticOp::CmpNEq;
		else if(op.type == TType::Plus)			ao = ArithmeticOp::Add;
		else if(op.type == TType::Minus)		ao = ArithmeticOp::Subtract;
		else if(op.type == TType::Asterisk)		ao = ArithmeticOp::Multiply;
		else if(op.type == TType::Divide)		ao = ArithmeticOp::Divide;

		else if(op.type == TType::LSquare && ps.front().type == TType::RSquare)
		{
			ps.eat();
			ao = ArithmeticOp::Subscript;

			Token fake;
			fake.pin = op.pin;
			fake.text = arithmeticOpToString(ps.cgi, ao);
			fake.type = TType::Identifier;

			FuncDecl* fd = parseFuncDeclUsingIdentifierToken(ps, fake);

			if(fd->ptype == pts::NamedType::create(VOID_TYPE_STRING))
				parserError("Subscript operator must return a value");

			if(fd->params.size() == 0)
				parserError("Subscript operator must take at least one argument");

			pts::Type* type = fd->ptype;

			// subscript operator is done a bit differently
			// i suspect some others like deref ('#') operator will be too
			// needs to be able to set read/write, so we take from swift
			// act like a computed property, with get and set style bodies.

			ComputedProperty* cprop = parseComputedProperty(ps, arithmeticOpToString(ps.cgi, ao), type, fd->attribs, fake);
			SubscriptOpOverload* oo = CreateAST_Pin(SubscriptOpOverload, fd->pin);

			oo->decl = fd;
			oo->getterBody = cprop->getter;
			oo->setterBody = cprop->setter;
			oo->setterArgName = cprop->setterArgName;

			return oo;
		}
		else if(op.type == TType::Equal || op.type == TType::PlusEq || op.type == TType::MinusEq || op.type == TType::MultiplyEq
			|| op.type == TType::DivideEq)
		{
			switch(op.type)
			{
				case TType::Equal:		ao = ArithmeticOp::Assign; break;
				case TType::PlusEq:		ao = ArithmeticOp::PlusEquals; break;
				case TType::MinusEq:	ao = ArithmeticOp::MinusEquals; break;
				case TType::MultiplyEq:	ao = ArithmeticOp::MultiplyEquals; break;
				case TType::DivideEq:	ao = ArithmeticOp::DivideEquals; break;

				default: iceAssert(0);
			}

			AssignOpOverload* aoo = CreateAST(AssignOpOverload, op, ao);

			Token fake;
			fake.pin = op.pin;
			fake.text = arithmeticOpToString(ps.cgi, ao);
			fake.type = TType::Identifier;

			// parse a func declaration.
			uint64_t attr = checkAndApplyAttributes(ps, Attr_VisPublic | Attr_VisInternal | Attr_VisPrivate);
			aoo->func = parseFuncUsingIdentifierToken(ps, fake);

			aoo->func->decl->attribs = attr;
			aoo->attribs = attr;

			return aoo;
		}
		else
		{
			if(ps.cgi->customOperatorMapRev.find(op.text.to_string()) != ps.cgi->customOperatorMapRev.end())
				ao = ps.cgi->customOperatorMapRev[op.text.to_string()];

			else
				parserError("Unsupported operator overload on operator '%s'", op.text.to_string().c_str());
		}


		OpOverload* oo = CreateAST(OpOverload, op, ao);

		Token fake;
		fake.pin = ps.currentPos;
		fake.text = arithmeticOpToString(ps.cgi, ao);
		fake.type = TType::Identifier;

		// parse a func declaration.
		uint64_t attr = checkAndApplyAttributes(ps, Attr_VisPublic | Attr_VisInternal | Attr_VisPrivate | Attr_CommutativeOp);
		oo->func = parseFuncUsingIdentifierToken(ps, fake);
		oo->func->decl->attribs = attr & ~Attr_CommutativeOp;
		oo->func->decl->pin = op.pin;

		oo->attribs = attr;

		// check number of arguments
		// 2 is binop, 1 is unaryop

		if(attr & Attr_CommutativeOp)
		{
			if(oo->func->decl->params.size() == 2 || (ps.structNestLevel > 0 && oo->func->decl->params.size() == 1))
			{
				oo->kind = OpOverload::OperatorKind::CommBinary;
			}
			else
			{
				parserError("Expected exactly 2 arguments for binary op (marked commutative, must be binary op)");
			}
		}
		else if(oo->func->decl->params.size() == 2 || (ps.structNestLevel > 0 && oo->func->decl->params.size() == 1))
		{
			oo->kind = OpOverload::OperatorKind::NonCommBinary;
		}
		else if(oo->func->decl->params.size() == 1)
		{
			iceAssert(0 && "enotsup");
			oo->kind = OpOverload::OperatorKind::PrefixUnary;
		}
		else
		{
			parserError(ps, "Invalid number of parameters to operator overload; expected 1 or 2, got %zu",
				oo->func->decl->params.size());
		}

		return oo;
	}


	std::string pinToString(Parser::Pin p)
	{
		char* buf = new char[1024];
		snprintf(buf, 1024, "(%s:%d:%d)", Compiler::getFilenameFromID(p.fileID).c_str(), p.line, p.col);

		std::string ret(buf);
		delete[] buf;

		return ret;
	}












	const Token& ParserState::front()
	{
		iceAssert(this->hasTokens());
		return this->tokens[this->index];
	}

	size_t ParserState::getRemainingTokens()
	{
		return this->tokens.size() - this->index;
	}

	bool ParserState::hasTokens()
	{
		return this->index < this->tokens.size();
	}

	bool ParserState::empty()
	{
		return !this->hasTokens();
	}

	const Token& ParserState::lookahead(size_t i)
	{
		iceAssert(this->index + i < this->tokens.size());
		return this->tokens[i + this->index];
	}

	const Token& ParserState::skip(size_t i)
	{
		iceAssert(this->index + i < this->tokens.size());
		this->index += i;

		return this->tokens[this->index];
	}

	const Token& ParserState::pop()
	{
		// returns the current front, then pops front.
		if(this->tokens.size() == 0)
			parserError(*this, "Unexpected end of input");

		const auto& t = this->tokens[this->index];
		this->index++;

		this->curtok = &t;
		return t;
	}

	const Token& ParserState::eat()
	{
		// returns the current front, then pops front.
		// this one skips newlines.
		if(this->tokens.size() == 0)
			parserError(*this, "Unexpected end of input");

		this->skipNewline();

		const auto& t = this->tokens[this->index];
		this->index++;

		// this->skipNewline();

		this->curtok = &t;
		return t;
	}

	void ParserState::skipNewline()
	{
		// eat newlines AND comments
		while(this->hasTokens() && this->front().type == TType::NewLine)
		{
			this->index++;
			this->currentPos.line++;
		}
	}

	void ParserState::reset()
	{
		this->index = 0;
	}

	const Token& ParserState::lookaheadUntilNonNewline()
	{
		size_t k = 0;
		while(this->index + k < this->tokens.size() && (this->lookahead(k).type == TType::Comment || this->lookahead(k).type == TType::NewLine))
			k++;

		return this->lookahead(k);
	}

	void ParserState::refundTokens(size_t i)
	{
		iceAssert(this->index >= i);
		this->index -= i;
	}

	ParserState::ParserState(Codegen::CodegenInstance* c, const TokenList& tl) : cgi(c), tokens(tl)
	{
	}
}

using namespace Parser;

static HighlightOptions pinToHO(Pin p)
{
	HighlightOptions ops;
	ops.caret = p;

	return ops;
}




void parserMessage(Err sev, const char* msg, ...) __attribute__((format(printf, 2, 3)));
void parserMessage(Err sev, const char* msg, ...)
{
	std::string str = "??";
	if(sev == Err::Info)		str = "Note";
	else if(sev == Err::Warn)	str = "Warning";
	else if(sev == Err::Error)	str = "Error";

	va_list ap;
	va_start(ap, msg);
	__error_gen(pinToHO(staticState->curtok->pin), msg, str.c_str(), false, ap);
	va_end(ap);
}

void parserMessage(Err sev, const Parser::Pin& pin, const char* msg, ...) __attribute__((format(printf, 3, 4)));
void parserMessage(Err sev, const Parser::Pin& pin, const char* msg, ...)
{

	std::string str = "??";
	if(sev == Err::Info)		str = "Note";
	else if(sev == Err::Warn)	str = "Warning";
	else if(sev == Err::Error)	str = "Error";

	va_list ap;
	va_start(ap, msg);
	__error_gen(pinToHO(pin), msg, str.c_str(), false, ap);
	va_end(ap);
}

void parserMessage(Err sev, const Parser::Token& tok, const char* msg, ...) __attribute__((format(printf, 3, 4)));
void parserMessage(Err sev, const Parser::Token& tok, const char* msg, ...)
{

	std::string str = "??";
	if(sev == Err::Info)		str = "Note";
	else if(sev == Err::Warn)	str = "Warning";
	else if(sev == Err::Error)	str = "Error";

	va_list ap;
	va_start(ap, msg);
	__error_gen(pinToHO(tok.pin), msg, str.c_str(), false, ap);
	va_end(ap);
}

void parserMessage(Err sev, Parser::ParserState& ps, const char* msg, ...) __attribute__((format(printf, 3, 4)));
void parserMessage(Err sev, Parser::ParserState& ps, const char* msg, ...)
{

	std::string str = "??";
	if(sev == Err::Info)		str = "Note";
	else if(sev == Err::Warn)	str = "Warning";
	else if(sev == Err::Error)	str = "Error";

	va_list ap;
	va_start(ap, msg);
	__error_gen(pinToHO(ps.curtok->pin), msg, str.c_str(), false, ap);
	va_end(ap);
}


void parserError(const char* msg, ...) __attribute__((format(printf, 1, 2), noreturn));
void parserError(const char* msg, ...)
{
	va_list ap;
	va_start(ap, msg);
	__error_gen(pinToHO(staticState->curtok->pin), msg, "Error", true, ap);
	va_end(ap);
	abort();
}

void parserError(const Parser::Pin& pin, const char* msg, ...) __attribute__((format(printf, 2, 3), noreturn));
void parserError(const Parser::Pin& pin, const char* msg, ...)
{
	va_list ap;
	va_start(ap, msg);
	__error_gen(pinToHO(pin), msg, "Error", true, ap);
	va_end(ap);
	abort();
}

void parserError(const Parser::Token& tok, const char* msg, ...) __attribute__((format(printf, 2, 3), noreturn));
void parserError(const Parser::Token& tok, const char* msg, ...)
{
	va_list ap;
	va_start(ap, msg);
	__error_gen(pinToHO(tok.pin), msg, "Error", true, ap);
	va_end(ap);
	abort();
}

void parserError(Parser::ParserState& ps, const char* msg, ...) __attribute__((format(printf, 2, 3), noreturn));
void parserError(Parser::ParserState& ps, const char* msg, ...)
{
	va_list ap;
	va_start(ap, msg);
	__error_gen(pinToHO(ps.curtok->pin), msg, "Error", true, ap);
	va_end(ap);
	abort();
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
