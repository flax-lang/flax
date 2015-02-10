// parser.h
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once


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

		Private,
		Public,
		Internal,

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

	struct PosInfo
	{
		uint64_t line;
		std::string file;
	};

	struct Token
	{
		PosInfo posinfo;
		std::string text;
		TType type;
	};


	void parserError(const char* msg, ...) __attribute__((noreturn));
	void parserWarn(const char* msg, ...);

	std::string getModuleName(std::string filename);
	Ast::Root* Parse(std::string filename, std::string str, Codegen::CodegenInstance* cgi);
	Token getNextToken(std::string& stream, PosInfo& pos);
	std::string arithmeticOpToString(Ast::ArithmeticOp op);

	Ast::ArithmeticOp mangledStringToOperator(std::string op);
	std::string operatorToMangledString(Ast::ArithmeticOp op);
}






