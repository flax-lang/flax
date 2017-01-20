// parser.h
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once

#include "ast.h"
#include "util.h"
#include "errors.h"

#include <deque>
#include <sstream>
#include <algorithm>
#include <experimental/string_view>

namespace Codegen
{
	struct CodegenInstance;
}

namespace Ast
{
	struct Root;
	enum class ArithmeticOp;
}


namespace Lexer
{
	void getNextToken(std::vector<std::experimental::string_view>& lines, size_t* line, const std::experimental::string_view& whole,
		Parser::Pin& pos, Parser::Token*);
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
		AsExclamation,
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
		Underscore,


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
		Number,
		StringLiteral,

		NewLine,
		Comment,



		EndOfFile,
	};

	using TokenList = util::FastVector<Token>;




	struct Token
	{
		Pin pin;
		TType type = TType::Invalid;
		std::experimental::string_view text;
	};

	struct ParserState
	{
		explicit ParserState(Codegen::CodegenInstance* c, const TokenList& tl);

		std::unordered_set<std::string> visited;

		const Token *curtok;
		Pin currentPos;
		Ast::Root* rootNode = 0;
		uint64_t curAttrib = 0;

		Codegen::CodegenInstance* cgi = 0;

		int leftParenNestLevel = 0;
		int currentOpPrec = 0;

		int structNestLevel = 0;

		bool empty();
		bool hasTokens();
		void skipNewline();

		size_t getRemainingTokens();

		const Token& pop();
		const Token& eat();
		const Token& front();

		void reset();
		const Token& skip(size_t i);
		const Token& lookahead(size_t i);

		private:

		const TokenList& tokens;
		size_t index = 0;
	};


	void setStaticState(ParserState& ps);





	void parseAll(ParserState& tokens);
	Ast::Expr* parsePrimary(ParserState& tokens);

	Ast::Expr* 				parseIf(ParserState& tokens);
	pts::Type*				parseType(ParserState& tokens);
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
	Ast::ProtocolDef*		parseProtocol(ParserState& tokens);
	Ast::Expr*				parseInitFunc(ParserState& tokens);
	Ast::Continue*			parseContinue(ParserState& tokens);
	Ast::FuncDecl*			parseFuncDecl(ParserState& tokens);
	void					parseAttribute(ParserState& tokens);
	Ast::TypeAlias*			parseTypeAlias(ParserState& tokens);
	Ast::ExtensionDef*		parseExtension(ParserState& tokens);
	Ast::NamespaceDecl*		parseNamespace(ParserState& tokens);
	Ast::Expr*				parseStaticDecl(ParserState& tokens);
	Ast::Expr*				parseOpOverload(ParserState& tokens);
	Ast::ForeignFuncDecl*	parseForeignFunc(ParserState& tokens);
	Ast::Func*				parseTopLevelExpr(ParserState& tokens);
	Ast::ArrayLiteral*		parseArrayLiteral(ParserState& tokens);
	Ast::Expr*				parseParenthesised(ParserState& tokens);
	Ast::StringLiteral*		parseStringLiteral(ParserState& tokens);
	Ast::Tuple*				parseTuple(ParserState& tokens, Ast::Expr* lhs);
	Ast::Expr*				parseRhs(ParserState& tokens, Ast::Expr* expr, int prio);
	Ast::FuncCall*			parseFuncCall(ParserState& tokens, std::string id, Pin id_pos);

	Ast::Func*				parseFuncUsingIdentifierToken(ParserState& tokens, Token id);
	Ast::FuncDecl*			parseFuncDeclUsingIdentifierToken(ParserState& tokens, Token id);
	Ast::BracedBlock*		parseBracedBlock(ParserState& tokens, bool hadOpeningBrace = false, bool eatClosingBrace = true);



	Ast::Root* Parse(Codegen::CodegenInstance* cgi, std::string filename);
	void parseAllCustomOperators(Codegen::CodegenInstance* cgi, std::string filename, std::string curpath);

	std::string pinToString(Parser::Pin p);

	std::string getModuleName(std::string filename);

	const std::string& arithmeticOpToString(Codegen::CodegenInstance*, Ast::ArithmeticOp op);
	Ast::ArithmeticOp mangledStringToOperator(Codegen::CodegenInstance*, std::string op);
	std::string operatorToMangledString(Codegen::CodegenInstance*, Ast::ArithmeticOp op);
}

















