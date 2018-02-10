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
		Using,                  // 50
		Operator,				//* 51
		LBrace,					//! 52
		RBrace,					// 53
		LParen,					// 54
		RParen,					// 55
		LSquare,				// 56
		RSquare,				// 57
		LAngle,					// 58
		RAngle,					// 59
		Plus,					// 60
		Minus,					// 61
		Asterisk,				// 62
		Divide,					// 63
		SQuote,					// 64
		DQuote,					// 65
		Period,					// 66
		Comma,					// 67
		Colon,					// 68
		Equal,					// 69
		Question,				// 70
		Exclamation,			// 71
		Semicolon,				// 72
		Ampersand,				// 73
		Percent,				// 74
		Pipe,					// 75
		Dollar,					// 76
		LogicalOr,				// 77
		LogicalAnd,				// 78
		At,						// 79
		Pound,					// 80
		Tilde,					// 81
		Caret,					// 82
		LeftArrow,				// 83
		RightArrow,				// 84
		FatLeftArrow,			// 85
		FatRightArrow,			// 86
		EqualsTo,				// 87
		NotEquals,				// 88
		GreaterEquals,			// 89
		LessThanEquals,			// 90
		ShiftLeft,				// 91
		ShiftRight,				// 92
		DoublePlus,				// 93
		DoubleMinus,			// 94
		PlusEq,					// 95
		MinusEq,				// 96
		MultiplyEq,				// 97
		DivideEq,				// 98
		ModEq,					// 99
		ShiftLeftEq,			// 100
		ShiftRightEq,			// 101
		AmpersandEq,			// 102
		PipeEq,					// 103
		CaretEq,				// 104
		Ellipsis,				// 105
		HalfOpenEllipsis,		// 106
		DoubleColon,			// 107
		Identifier,				// 108
		UnicodeSymbol,			// 109
		Number,					// 110
		StringLiteral,			// 111
		NewLine,				// 112
		Comment,				// 113
		EndOfFile,				// 114

		Attr_Raw,				// 115
		Attr_EntryFn,			// 116
		Attr_NoMangle,			// 117
		Attr_Operator,			// 118
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




















