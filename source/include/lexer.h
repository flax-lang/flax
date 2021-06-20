// lexer.h
// Copyright (c) 2014 - 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#pragma once
#include "defs.h"
#include "container.h"

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
		If,
		Else,
		Return,
		As,
		AsExclamation,
		Is,
		Switch,
		Case,
		Match,
		Enum,
		ForeignFunc,
		Struct,
		Union,
		Static,
		True,
		False,
		For,
		While,
		Do,
		Defer,
		Break,
		Continue,
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
		Trait,
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
		DoublePlus,
		DoubleMinus,
		PlusEq,
		MinusEq,
		MultiplyEq,
		DivideEq,
		ModEq,
		AmpersandEq,
		PipeEq,
		CaretEq,
		Ellipsis,
		HalfOpenEllipsis,
		DoubleColon,
		Identifier,
		UnicodeSymbol,
		IntegerNumber,
		FloatingNumber,
		StringLiteral,
		CharacterLiteral,
		NewLine,
		Comment,
		EndOfFile,

		Attr_ATTRS_BEGIN,

		Attr_Raw,
		Attr_Packed,
		Attr_EntryFn,
		Attr_NoMangle,
		Attr_Operator,

		Attr_ATTRS_END,

		Directive_Run,
		Directive_If,
	};

	struct Token
	{
		Location loc;
		TokenType type = TokenType::Invalid;
		std::string_view text;

		operator TokenType() const { return this->type; }
		bool operator == (const std::string& s) { return this->str() == s; }
		std::string str() const { return std::string(this->text); }
	};

	using TokenList = util::FastInsertVector<Token>;

	lexer::TokenType getNextToken(const util::FastInsertVector<std::string_view>& lines, size_t* line, size_t* offset,
		const std::string_view& whole, Location& pos, Token* out, bool crlf);
}




















