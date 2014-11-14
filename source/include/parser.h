// parser.h
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once


#include <string>
#include <sstream>
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
		Is,					// 14


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
		Ampersand,			// 36

		// compound symbols
		Arrow,
		EqualsTo,
		NotEquals,
		GreaterEquals,
		LessThanEquals,		// 41

		// other stuff.
		Identifier,
		Integer,
		Decimal,

		NewLine,
		Comment,
	};

	struct PosInfo
	{
		~PosInfo()
		{
			delete this->file;
		}

		uint64_t line;
		std::string* file;
	};

	struct Token
	{
		~Token()
		{
			delete this->posinfo;
		}

		PosInfo* posinfo;
		std::string text;
		TType type;
	};

	void Parse(std::string filename);
	Token* getNextToken(std::string& stream, PosInfo& pos);
}







