// lexer.h
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once
#include "defs.h"

namespace lexer
{
	enum class TokenType
	{
		Invalid,				// 0
		Func,					// 1
		Class,					// 2
		Import,					// 3
		Var,					// 4
		Val,					// 5
		Ptr,					// 6
		Deref,					// 7
		Addr,					// 8
		If,						// 9
		Else,					// 10
		Return,					// 11
		As,						// 12
		AsExclamation,			// 13
		Is,						// 14
		Switch,					// 15
		Case,					// 16
		Match,					// 17
		To,						// 18
		Enum,					// 19
		ForeignFunc,			// 20
		Struct,					// 21
		Static,					// 22
		True,					// 23
		False,					// 24
		For,					// 25
		While,					// 26
		Do,						// 27
		Loop,					// 28
		Defer,					// 29
		Break,					// 30
		Continue,				// 31
		Get,					// 32
		Set,					// 33
		Null,					// 34
		Private,				// 35
		Public,					// 36
		Internal,				// 37
		Extension,				// 38
		TypeAlias,				// 39
		Typeof,					// 40
		Typeid,					// 41
		Sizeof,					// 42
		Alloc,					// 43
		Dealloc,				// 44
		Export,					// 45
		Namespace,				// 46
		Override,				// 47
		Protocol,				// 48
		Virtual,                // 49
		Operator,				//* 50
		LBrace,					//! 51
		RBrace,					// 52
		LParen,					// 53
		RParen,					// 54
		LSquare,				// 55
		RSquare,				// 56
		LAngle,					// 57
		RAngle,					// 58
		Plus,					// 59
		Minus,					// 60
		Asterisk,				// 61
		Divide,					// 62
		SQuote,					// 63
		DQuote,					// 64
		Period,					// 65
		Comma,					// 66
		Colon,					// 67
		Equal,					// 68
		Question,				// 69
		Exclamation,			// 70
		Semicolon,				// 71
		Ampersand,				// 72
		Percent,				// 73
		Pipe,					// 74
		Dollar,					// 75
		LogicalOr,				// 76
		LogicalAnd,				// 77
		At,						// 78
		Pound,					// 79
		Tilde,					// 80
		Caret,					// 81
		LeftArrow,				// 82
		RightArrow,				// 83
		FatLeftArrow,			// 84
		FatRightArrow,			// 85
		EqualsTo,				// 86
		NotEquals,				// 87
		GreaterEquals,			// 88
		LessThanEquals,			// 89
		ShiftLeft,				// 90
		ShiftRight,				// 91
		DoublePlus,				// 92
		DoubleMinus,			// 93
		PlusEq,					// 94
		MinusEq,				// 95
		MultiplyEq,				// 96
		DivideEq,				// 97
		ModEq,					// 98
		ShiftLeftEq,			// 99
		ShiftRightEq,			// 100
		AmpersandEq,			// 101
		PipeEq,					// 102
		CaretEq,				// 103
		Ellipsis,				// 104
		HalfOpenEllipsis,		// 105
		DoubleColon,			// 106
		Identifier,				// 107
		UnicodeSymbol,			// 108
		Number,					// 109
		StringLiteral,			// 110
		NewLine,				// 111
		Comment,				// 112
		EndOfFile,				// 113

		Attr_Raw,				// 114
		Attr_EntryFn,			// 115
		Attr_NoMangle,			// 116
		Attr_Operator,			// 117
	};

	struct Token
	{
		Location loc;
		TokenType type = TokenType::Invalid;
		util::string_view text;

		operator TokenType() const { return this->type; }
		bool operator == (const std::string& s) { return this->str() == s; }
		std::string str() const { return util::to_string(this->text); }
	};

	inline void operator << (std::ostream& os, const TokenType& tt)
	{
		os << (int) tt;
	}

	using TokenList = util::FastVector<Token>;

	lexer::TokenType getNextToken(const util::FastVector<util::string_view>& lines, size_t* line, size_t* offset,
		const util::string_view& whole, Location& pos, Token* out, bool crlf);
}




















