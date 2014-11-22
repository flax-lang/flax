// parser.h
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once


#include <string>
#include <sstream>

namespace Ast
{
	class Root;
	enum class VarType;
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
		For,
		While,
		If,
		Else,
		Return,
		As,
		Is,
		Switch,
		Case,
		Enum,				// 17
		ForeignFunc,
		Struct,
		True,
		False,

		Private,
		Public,
		Internal,


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
		Percent,			// 48
		Pipe,
		LogicalOr,
		LogicalAnd,
		At,

		// compound symbols
		Arrow,
		EqualsTo,
		NotEquals,
		GreaterEquals,
		LessThanEquals,		// 55
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

	std::string getModuleName();
	Ast::Root* Parse(std::string filename, std::string str);
	Token* getNextToken(std::string& stream, PosInfo& pos);
	Ast::VarType determineVarType(std::string type_id);
}






