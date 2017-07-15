// lexer.h
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once
#include "defs.h"

namespace lexer
{
	enum class TokenType
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
		Typeid,
		Sizeof,
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

		Ellipsis,			// ...
		HalfOpenEllipsis,	// ..<
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

	struct Token
	{
		Location loc;
		TokenType type = TokenType::Invalid;
		stx::string_view text;
	};

	using TokenList = util::FastVector<Token>;

	void getNextToken(std::vector<stx::string_view>& lines, size_t* line, const stx::string_view& whole, Location& pos, Token* tok);
}




















