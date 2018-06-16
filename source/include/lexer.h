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
		Export,
		Namespace,
		Override,
		Protocol,
		Virtual,
		Using,
		Mutable,
		Operator,
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
		Dollar,
		LogicalOr,
		LogicalAnd,
		At,
		Pound,
		Tilde,
		Caret,
		LeftArrow,
		RightArrow,
		FatLeftArrow,
		FatRightArrow,
		EqualsTo,
		NotEquals,
		GreaterEquals,
		LessThanEquals,
		ShiftLeft,
		ShiftRight,
		DoublePlus,
		DoubleMinus,
		PlusEq,
		MinusEq,
		MultiplyEq,
		DivideEq,
		ModEq,
		ShiftLeftEq,
		ShiftRightEq,
		AmpersandEq,
		PipeEq,
		CaretEq,
		Ellipsis,
		HalfOpenEllipsis,
		DoubleColon,
		Identifier,
		UnicodeSymbol,
		Number,
		StringLiteral,
		NewLine,
		Comment,
		EndOfFile,

		Attr_Raw,
		Attr_EntryFn,
		Attr_NoMangle,
		Attr_Operator,
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




















