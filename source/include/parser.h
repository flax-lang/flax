// parser.h
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once


#include "ast.h"
#include <string>
#include <sstream>

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
		True,
		False,

		For,
		While,
		Do,
		Loop,

		Break,
		Continue,

		Get,
		Set,

		Private,
		Public,
		Internal,

		Extension,

		BuiltinType,
		Alloc,
		Dealloc,

		Module,
		Namespace,


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
		Elipsis,
		DoubleColon,

		// other stuff.
		Identifier,
		Integer,
		Decimal,
		StringLiteral,

		NewLine,
		Comment,
	};


	struct Token
	{
		PosInfo posinfo;
		std::string text;
		TType type;
	};


	void parserError(const char* msg, ...) __attribute__((noreturn));
	void parserWarn(const char* msg, ...);






	void parseAll(std::deque<Token>& tokens);
	Ast::Expr* parseIf(std::deque<Token>& tokens);
	void parseAttribute(std::deque<Token>& tokens);
	Ast::Func* parseFunc(std::deque<Token>& tokens);
	Ast::Expr* parseExpr(std::deque<Token>& tokens);
	Ast::Expr* parseUnary(std::deque<Token>& tokens);
	Ast::Alloc* parseAlloc(std::deque<Token>& tokens);
	Ast::ForLoop* parseFor(std::deque<Token>& tokens);
	Ast::Expr* parseIdExpr(std::deque<Token>& tokens);
	Ast::Break* parseBreak(std::deque<Token>& tokens);
	Ast::Expr* parsePrimary(std::deque<Token>& tokens);
	Ast::Expr* parseInitFunc(std::deque<Token>& tokens);
	Ast::Struct* parseStruct(std::deque<Token>& tokens);
	Ast::Import* parseImport(std::deque<Token>& tokens);
	Ast::Return* parseReturn(std::deque<Token>& tokens);
	Ast::Number* parseNumber(std::deque<Token>& tokens);
	Ast::CastedType* parseType(std::deque<Token>& tokens);
	Ast::VarDecl* parseVarDecl(std::deque<Token>& tokens);
	Ast::WhileLoop* parseWhile(std::deque<Token>& tokens);
	Ast::Dealloc* parseDealloc(std::deque<Token>& tokens);
	Ast::Continue* parseContinue(std::deque<Token>& tokens);
	Ast::Func* parseTopLevelExpr(std::deque<Token>& tokens);
	Ast::FuncDecl* parseFuncDecl(std::deque<Token>& tokens);
	Ast::Expr* parseParenthesised(std::deque<Token>& tokens);
	Ast::Extension* parseExtension(std::deque<Token>& tokens);
	Ast::OpOverload* parseOpOverload(std::deque<Token>& tokens);
	Ast::NamespaceDecl* parseNamespace(std::deque<Token>& tokens);
	Ast::BracedBlock* parseBracedBlock(std::deque<Token>& tokens);
	Ast::StringLiteral* parseStringLiteral(std::deque<Token>& tokens);
	Ast::ForeignFuncDecl* parseForeignFunc(std::deque<Token>& tokens);
	Ast::Expr* parseFuncCall(std::deque<Token>& tokens, std::string id);
	Ast::Expr* parseRhs(std::deque<Token>& tokens, Ast::Expr* expr, int prio);












	std::string getModuleName(std::string filename);
	Ast::Root* Parse(std::string filename, std::string str, Codegen::CodegenInstance* cgi);
	Token getNextToken(std::string& stream, PosInfo& pos);
	std::string arithmeticOpToString(Ast::ArithmeticOp op);

	Ast::ArithmeticOp mangledStringToOperator(std::string op);
	std::string operatorToMangledString(Ast::ArithmeticOp op);
}






