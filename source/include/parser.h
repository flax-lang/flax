// parser.h
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once


#include <string>
#include <sstream>

namespace Codegen
{
	class CodegenInstance;
}

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
		If,
		Else,
		Return,
		As,
		Is,
		Switch,
		Case,
		Match,
		To,
		Enum,				// 17
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
		Pound,

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
		Elipsis,

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

	std::string getModuleName(std::string filename);
	Ast::Root* Parse(std::string filename, std::string str, Codegen::CodegenInstance* cgi);
	Token* getNextToken(std::string& stream, PosInfo& pos);
	Ast::VarType determineVarType(std::string type_id);
}






