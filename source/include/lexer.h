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
		Module,					// 45
		Namespace,				// 46
		Override,				// 47
		Protocol,				// 48
		Operator,				// 49
		LBrace,					// 50
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
		LogicalOr,				// 74
		LogicalAnd,				// 75
		At,						// 76
		Pound,					// 77
		Tilde,					// 78
		Caret,					// 79
		Underscore,				// 80
		Arrow,					// 81
		EqualsTo,				// 82
		NotEquals,				// 83
		GreaterEquals,			// 84
		LessThanEquals,			// 85
		ShiftLeft,				// 86
		ShiftRight,				// 87
		DoublePlus,				// 88
		DoubleMinus,			// 89
		PlusEq,					// 90
		MinusEq,				// 91
		MultiplyEq,				// 92
		DivideEq,				// 93
		ModEq,					// 94
		ShiftLeftEq,			// 95
		ShiftRightEq,			// 96
		AmpersandEq,			// 97
		PipeEq,					// 98
		CaretEq,				// 99
		Ellipsis,				// 100
		HalfOpenEllipsis,		// 101
		DoubleColon,			// 102
		Identifier,				// 103
		UnicodeSymbol,			// 104
		Number,					// 105
		StringLiteral,			// 106
		NewLine,				// 107
		Comment,				// 108
		EndOfFile,				// 109
	};

	struct Token
	{
		Location loc;
		TokenType type = TokenType::Invalid;
		stx::string_view text;

		operator TokenType() const { return this->type; }
		std::string str() const { return this->text.to_string(); }
	};

	using TokenList = util::FastVector<Token>;

	lexer::TokenType getNextToken(const util::FastVector<stx::string_view>& lines, size_t* line, size_t* offset,
		const stx::string_view& whole, Location& pos, Token* out);
}




















