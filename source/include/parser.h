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

		Operator,

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
		Ast::Root* rootNode = 0;
		uint64_t curAttrib = 0;

		Codegen::CodegenInstance* cgi = 0;

		int leftParenNestLevel = 0;
		int currentOpPrec = 0;

		int structNestLevel = 0;

		bool empty();
		bool hasTokens();
		void skipNewline();

		// once broken considered sold
		void refundToPosition(size_t index);

		size_t getRemainingTokens();

		const Token& pop();
		const Token& eat();
		const Token& front();

		void reset();
		const Token& skip(size_t i);
		const Token& lookahead(size_t i);
		const Token& lookaheadUntilNonNewline();

		// private:

		const TokenList& tokens;
		size_t index = 0;
	};


	void setStaticState(ParserState& ps);





	void parseAll(ParserState& tokens);
	void parseAttribute(ParserState& ps);
	Ast::Expr* parsePrimary(ParserState& ps);
	Ast::Expr* parseStatement(ParserState& ps, bool allowingImports = false);


	Ast::Expr* 					parseIf(ParserState& ps);
	pts::Type*					parseType(ParserState& ps);
	Ast::EnumDef*				parseEnum(ParserState& ps);
	Ast::Func*					parseFunc(ParserState& ps);
	Ast::Expr*					parseExpr(ParserState& ps);
	Ast::Expr*					parseUnary(ParserState& ps);
	Ast::WhileLoop*				parseWhile(ParserState& ps);
	Ast::Alloc*					parseAlloc(ParserState& ps);
	Ast::Break*					parseBreak(ParserState& ps);
	Ast::DeferredExpr*			parseDefer(ParserState& ps);
	Ast::ClassDef*				parseClass(ParserState& ps);
	Ast::Expr*					parseIdExpr(ParserState& ps);
	Ast::StructDef*				parseStruct(ParserState& ps);
	Ast::Import*				parseImport(ParserState& ps);
	Ast::Return*				parseReturn(ParserState& ps);
	Ast::Typeof*				parseTypeof(ParserState& ps);
	Ast::Number*				parseNumber(ParserState& ps);
	Ast::Expr*					parseVarDecl(ParserState& ps);
	Ast::Dealloc*				parseDealloc(ParserState& ps);
	Ast::ProtocolDef*			parseProtocol(ParserState& ps);
	Ast::Expr*					parseInitFunc(ParserState& ps);
	Ast::Continue*				parseContinue(ParserState& ps);
	Ast::FuncDecl*				parseFuncDecl(ParserState& ps);
	Ast::TypeAlias*				parseTypeAlias(ParserState& ps);
	Ast::ExtensionDef*			parseExtension(ParserState& ps);
	Ast::NamespaceDecl*			parseNamespace(ParserState& ps);
	Ast::Expr*					parseStaticDecl(ParserState& ps);
	Ast::Expr*					parseOpOverload(ParserState& ps);
	Ast::ForeignFuncDecl*		parseForeignFunc(ParserState& ps);
	Ast::Func*					parseTopLevelExpr(ParserState& ps);
	Ast::ArrayLiteral*			parseArrayLiteral(ParserState& ps);
	Ast::Expr*					parseParenthesised(ParserState& ps);
	Ast::StringLiteral*			parseStringLiteral(ParserState& ps);
	Ast::TupleDecompDecl*		parseTupleDecomposition(ParserState& ps);
	Ast::ArrayDecompDecl*		parseArrayDecomposition(ParserState& ps);
	Ast::Tuple*					parseTuple(ParserState& ps, Ast::Expr* lhs);
	Ast::Expr*					parseRhs(ParserState& ps, Ast::Expr* expr, int prio);
	Ast::FuncCall*				parseFuncCall(ParserState& ps, std::string id, Pin id_pos);

	Ast::Func*					parseFuncUsingIdentifierToken(ParserState& ps, Token id);
	Ast::FuncDecl*				parseFuncDeclUsingIdentifierToken(ParserState& ps, Token id);
	Ast::BracedBlock*			parseBracedBlock(ParserState& ps, bool hadOpeningBrace = false, bool eatClosingBrace = true);



	Ast::Root* Parse(Codegen::CodegenInstance* cgi, std::string filename);
	void parseAllCustomOperators(Codegen::CodegenInstance* cgi, std::string filename, std::string curpath);

	std::string pinToString(Parser::Pin p);

	std::string getModuleName(std::string filename);

	const std::string& arithmeticOpToString(Codegen::CodegenInstance*, Ast::ArithmeticOp op);
	Ast::ArithmeticOp mangledStringToOperator(Codegen::CodegenInstance*, std::string op);
	std::string operatorToMangledString(Codegen::CodegenInstance*, Ast::ArithmeticOp op);
}

















