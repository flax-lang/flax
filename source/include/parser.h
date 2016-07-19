// parser.h
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once

#include "ast.h"
#include "errors.h"

#include <string>
#include <sstream>
#include <deque>
#include <algorithm>

namespace Codegen
{
	struct CodegenInstance;
}

namespace Ast
{
	struct Root;
	enum class ArithmeticOp;
}


namespace Parser
{
	enum class TType
	{
		// keywords
		Invalid,
		Func,
		Class,
		Import,
		Var,
		Val,
		Ptr,
		Deref,
		Addr,
		If,
		Else,
		Return,
		As,
		Is,
		Switch,
		Case,
		Match,
		To,
		Enum,
		ForeignFunc,
		Struct,
		Static,
		True,
		False,

		For,
		While,
		Do,
		Loop,
		Defer,

		Break,
		Continue,

		Get,
		Set,

		Null,

		Private,
		Public,
		Internal,

		Extension,
		TypeAlias,

		Typeof,
		Alloc,
		Dealloc,

		Module,
		Namespace,
		Override,
		Protocol,

		// symbols
		LBrace,
		RBrace,
		LParen,
		RParen,
		LSquare,
		RSquare,
		LAngle,
		RAngle,
		Plus,
		Minus,
		Asterisk,
		Divide,
		SQuote,
		DQuote,
		Period,
		Comma,
		Colon,
		Equal,
		Question,
		Exclamation,
		Semicolon,
		Ampersand,
		Percent,
		Pipe,
		LogicalOr,
		LogicalAnd,
		At,
		Pound,
		Tilde,
		Caret,


		// compound symbols
		Arrow,
		EqualsTo,
		NotEquals,
		GreaterEquals,
		LessThanEquals,
		ShiftLeft,
		ShiftRight,
		DoublePlus,			// doubleplusgood
		DoubleMinus,
		PlusEq,
		MinusEq,
		MultiplyEq,
		DivideEq,
		ModEq,
		ShiftLeftEq,
		ShiftRightEq,
		AmpersandEq,		// &= (AND)
		PipeEq,				// |= (OR)
		CaretEq,			// ^= (XOR)

		Ellipsis,
		DoubleColon,

		// other stuff.
		Identifier,
		UnicodeSymbol,
		Integer,
		Decimal,
		StringLiteral,

		NewLine,
		Comment,
	};


	struct Token
	{
		Token() { }

		Pin pin;
		std::string text;
		TType type = TType::Invalid;
	};


	typedef std::deque<Token> TokenList;
	struct ParserState
	{
		explicit ParserState(Codegen::CodegenInstance* c) : cgi(c) { }

		TokenList tokens;

		std::map<std::string, bool> visited;

		Token curtok;
		Pin currentPos;
		Ast::Root* rootNode = 0;
		uint64_t curAttrib = 0;

		Codegen::CodegenInstance* cgi = 0;

		bool isParsingStruct = false;
		bool didHaveLeftParen = false;
		int currentOpPrec = 0;


		Token front()
		{
			return this->tokens.front();
		}

		void pop_front()
		{
			this->tokens.pop_front();
		}

		Token eat()
		{
			// returns the current front, then pops front.
			if(this->tokens.size() == 0)
				parserError(*this, "Unexpected end of input");

			this->skipNewline();
			Token t = this->front();
			this->pop_front();

			this->skipNewline();

			this->curtok = t;
			return t;
		}

		void skipNewline()
		{
			// eat newlines AND comments
			while(this->tokens.size() > 0 && (this->tokens.front().type == TType::NewLine
				|| this->tokens.front().type == TType::Comment || this->tokens.front().type == TType::Semicolon))
			{
				this->tokens.pop_front();
				this->currentPos.line++;
			}
		}
	};








	void parseAll(ParserState& tokens);
	Ast::Expr* parsePrimary(ParserState& tokens);

	Ast::Expr* 				parseIf(ParserState& tokens);
	Ast::ForLoop*			parseFor(ParserState& tokens);
	Ast::Expr*				parseType(ParserState& tokens);
	Ast::EnumDef*			parseEnum(ParserState& tokens);
	Ast::Func*				parseFunc(ParserState& tokens);
	Ast::Expr*				parseExpr(ParserState& tokens);
	Ast::Expr*				parseUnary(ParserState& tokens);
	Ast::WhileLoop*			parseWhile(ParserState& tokens);
	Ast::Alloc*				parseAlloc(ParserState& tokens);
	Ast::Break*				parseBreak(ParserState& tokens);
	Ast::DeferredExpr*		parseDefer(ParserState& tokens);
	Ast::ClassDef*			parseClass(ParserState& tokens);
	Ast::Expr*				parseIdExpr(ParserState& tokens);
	Ast::StructDef*			parseStruct(ParserState& tokens);
	Ast::Import*			parseImport(ParserState& tokens);
	Ast::Return*			parseReturn(ParserState& tokens);
	Ast::Typeof*			parseTypeof(ParserState& tokens);
	Ast::Number*			parseNumber(ParserState& tokens);
	Ast::VarDecl*			parseVarDecl(ParserState& tokens);
	Ast::Dealloc*			parseDealloc(ParserState& tokens);
	Ast::Expr*				parseInitFunc(ParserState& tokens);
	Ast::Continue*			parseContinue(ParserState& tokens);
	Ast::FuncDecl*			parseFuncDecl(ParserState& tokens);
	void					parseAttribute(ParserState& tokens);
	Ast::TypeAlias*			parseTypeAlias(ParserState& tokens);
	Ast::ExtensionDef*		parseExtension(ParserState& tokens);
	Ast::NamespaceDecl*		parseNamespace(ParserState& tokens);
	Ast::Expr*				parseStaticDecl(ParserState& tokens);
	Ast::Expr*				parseOpOverload(ParserState& tokens);
	Ast::BracedBlock*		parseBracedBlock(ParserState& tokens);
	Ast::ForeignFuncDecl*	parseForeignFunc(ParserState& tokens);
	Ast::Func*				parseTopLevelExpr(ParserState& tokens);
	Ast::ArrayLiteral*		parseArrayLiteral(ParserState& tokens);
	Ast::Expr*				parseParenthesised(ParserState& tokens);
	Ast::StringLiteral*		parseStringLiteral(ParserState& tokens);
	Ast::Tuple*				parseTuple(ParserState& tokens, Ast::Expr* lhs);
	Ast::Expr*				parseRhs(ParserState& tokens, Ast::Expr* expr, int prio);
	Ast::FuncCall*			parseFuncCall(ParserState& tokens, std::string id, Pin id_pos);



	Ast::Root* Parse(ParserState& pstate, std::string filename);
	void parseAllCustomOperators(ParserState& pstate, std::string filename, std::string curpath);




	std::string getModuleName(std::string filename);
	Token getNextToken(std::string& stream, Pin& pos);

	std::string arithmeticOpToString(Codegen::CodegenInstance*, Ast::ArithmeticOp op);
	Ast::ArithmeticOp mangledStringToOperator(Codegen::CodegenInstance*, std::string op);
	std::string operatorToMangledString(Codegen::CodegenInstance*, Ast::ArithmeticOp op);
}

















