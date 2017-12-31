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
		Operator,				//* 49
		LBrace,					//! 50
		RBrace,					// 51
		LParen,					// 52
		RParen,					// 53
		LSquare,				// 54
		RSquare,				// 55
		LAngle,					// 56
		RAngle,					// 57
		Plus,					// 58
		Minus,					// 59
		Asterisk,				// 60
		Divide,					// 61
		SQuote,					// 62
		DQuote,					// 63
		Period,					// 64
		Comma,					// 65
		Colon,					// 66
		Equal,					// 67
		Question,				// 68
		Exclamation,			// 69
		Semicolon,				// 70
		Ampersand,				// 71
		Percent,				// 72
		Pipe,					// 73
		Dollar,					// 74
		LogicalOr,				// 75
		LogicalAnd,				// 76
		At,						// 77
		Pound,					// 78
		Tilde,					// 79
		Caret,					// 80
		Underscore,				// 81
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




















