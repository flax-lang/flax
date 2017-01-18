// Lexer.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include <string>
#include <ctype.h>
#include <cassert>
#include <iostream>

#include "parser.h"
#include "../utf8rewind/include/utf8rewind/utf8rewind.h"

namespace Parser
{
	static void skipWhitespace(std::experimental::string_view& line, Pin& pos)
	{
		size_t startpos = line.find_first_not_of(" \t");
		if(startpos != std::string::npos)
		{
			for(size_t i = 0; i < startpos; i++)
			{
				if(line[i] == ' ')			pos.col++;
				else if(line[i] == '\t')	pos.col += TAB_WIDTH;
			}

			line = line.substr(startpos);
		}
	}

	static Token previousToken;
	static bool shouldConsiderUnaryLiteral(std::experimental::string_view& stream, Pin& pos)
	{
		// check the previous token
		bool res = (previousToken.type != TType::Invalid && previousToken.pin.file == pos.file &&
			(previousToken.type != TType::RParen && previousToken.type != TType::RSquare && previousToken.type != TType::Identifier
			&& previousToken.type != TType::Number));

		if(!res) return false;

		// check if the current char is a + or -
		if(stream.length() == 0) return false;
		if(stream[0] != '+' && stream[0] != '-') return false;

		// check if there's only spaces between this and the number itself
		for(size_t i = 1; i < stream.length(); i++)
		{
			if(isdigit(stream[i])) return true;
			else if(stream[i] != ' ') return false;
		}

		return false;
	}


	Token getNextToken(std::experimental::string_view& stream, Pin& pos)
	{
		// auto p = prof::Profile("get 1 token");

		if(stream.length() == 0)
		{
			Token ret;
			ret.type = TType::EndOfFile;
			return ret;
		}

		size_t read = 0;
		size_t unicodeLength = 0;

		// first eat all whitespace
		skipWhitespace(stream, pos);

		Token tok;
		tok.pin = pos;

		// check compound symbols first.
		if(stream.compare(0, 2, "//") == 0)
		{
			size_t newline = stream.find('\n');


			tok.type = TType::Comment;
			tok.text = stream.substr(0, newline).to_string();
			read = tok.text.length();
			pos.line++;
		}
		else if(stream.compare(0, 2, "==") == 0)
		{
			tok.text = "==";
			tok.type = TType::EqualsTo;
			read = 2;
		}
		else if(stream.compare(0, 2, ">=") == 0)
		{
			tok.text = ">=";
			tok.type = TType::GreaterEquals;
			read = 2;
		}
		else if(stream.compare(0, 2, "<=") == 0)
		{
			tok.text = "<=";
			tok.type = TType::LessThanEquals;
			read = 2;
		}
		else if(stream.compare(0, 2, "!=") == 0)
		{
			tok.text = "!=";
			tok.type = TType::NotEquals;
			read = 2;
		}
		else if(stream.compare(0, 2, "||") == 0)
		{
			tok.text = "||";
			tok.type = TType::LogicalOr;
			read = 2;
		}
		else if(stream.compare(0, 2, "&&") == 0)
		{
			tok.text = "&&";
			tok.type = TType::LogicalAnd;
			read = 2;
		}
		else if(stream.compare(0, 2, "->") == 0)
		{
			tok.text = "->";
			tok.type = TType::Arrow;
			read = 2;
		}
		else if(stream.compare(0, 2, "++") == 0)
		{
			tok.text = "++";
			tok.type = TType::DoublePlus;
			read = 2;
		}
		else if(stream.compare(0, 2, "--") == 0)
		{
			tok.text = "--";
			tok.type = TType::DoubleMinus;
			read = 2;
		}
		else if(stream.compare(0, 2, "+=") == 0)
		{
			tok.text = "+=";
			tok.type = TType::PlusEq;
			read = 2;
		}
		else if(stream.compare(0, 2, "-=") == 0)
		{
			tok.text = "+=";
			tok.type = TType::MinusEq;
			read = 2;
		}
		else if(stream.compare(0, 2, "*=") == 0)
		{
			tok.text = "+=";
			tok.type = TType::MultiplyEq;
			read = 2;
		}
		else if(stream.compare(0, 2, "/=") == 0)
		{
			tok.text = "+=";
			tok.type = TType::DivideEq;
			read = 2;
		}
		else if(stream.compare(0, 2, "%=") == 0)
		{
			tok.text = "%=";
			tok.type = TType::ModEq;
			read = 2;
		}
		else if(stream.compare(0, 2, "&=") == 0)
		{
			tok.text = "&=";
			tok.type = TType::AmpersandEq;
			read = 2;
		}
		else if(stream.compare(0, 2, "|=") == 0)
		{
			tok.text = "|=";
			tok.type = TType::PipeEq;
			read = 2;
		}
		else if(stream.compare(0, 2, "^=") == 0)
		{
			tok.text = "^=";
			tok.type = TType::CaretEq;
			read = 2;
		}
		else if(stream.compare(0, 3, "...") == 0)
		{
			tok.text = "...";
			tok.type = TType::Ellipsis;
			read = 3;
		}
		else if(stream.compare(0, 2, "/*") == 0)
		{
			int currentNest = 1;

			// support nested, so basically we have to loop until we find either a /* or a */
			stream = stream.substr(2);

			pos.col += 2;

			Pin opening = pos;
			Pin curpos = pos;

			size_t k = 0;
			while(currentNest > 0 && k < stream.size())
			{
				if(k + 1 == stream.size())
					parserError(opening, "Expected closing */ (reached EOF), for block comment started here:");

				if(stream[k] == '\n')
					curpos.line++;

				if(stream[k] == '/' && stream[k + 1] == '*')
					currentNest++, k++, curpos.col++, opening = curpos;

				else if(stream[k] == '*' && stream[k + 1] == '/')
					currentNest--, k++, curpos.col++;

				k++;
				curpos.col++;
			}

			stream = stream.substr(k);
			pos = curpos;

			return getNextToken(stream, pos);
		}
		else if(stream.compare(0, 2, "*/") == 0)
		{
			parserError(tok, "Unexpected '*/'");
		}

		// unicode stuff
		else if(stream.compare(0, strlen("ƒ"), "ƒ") == 0)
		{
			tok.text = FUNC_KEYWORD_STRING;
			tok.type = TType::Func;
			read = std::string("ƒ").length();

			unicodeLength = 1;
		}
		else if(stream.compare(0, strlen("ﬁ"), "ﬁ") == 0)
		{
			tok.text = "ffi";
			tok.type = TType::ForeignFunc;
			read = std::string("ﬁ").length();

			unicodeLength = 1;
		}
		else if(stream.compare(0, strlen("÷"), "÷") == 0)
		{
			tok.text = "÷";
			tok.type = TType::Divide;
			read = std::string("÷").length();

			unicodeLength = 1;
		}
		else if(stream.compare(0, strlen("≠"), "≠") == 0)
		{
			tok.text = "≠";
			tok.type = TType::NotEquals;
			read = std::string("≠").length();

			unicodeLength = 1;
		}
		else if(stream.compare(0, strlen("≤"), "≤") == 0)
		{
			tok.text = "≤";
			tok.type = TType::LessThanEquals;
			read = std::string("≤").length();

			unicodeLength = 1;
		}
		else if(stream.compare(0, strlen("≥"), "≥") == 0)
		{
			tok.text = "≥";
			tok.type = TType::GreaterEquals;
			read = std::string("≥").length();

			unicodeLength = 1;
		}

		// note some special-casing is needed to differentiate between unary +/- and binary +/-
		// cases where we want binary:
		// ...) + 3
		// ...] + 3
		// ident + 3
		// number + 3
		// so in every other case we want unary +/-.

		// note(2):
		// here's how this shit fucks up:
		// since numbers are handled at the lexing level, we have no idea about precedence.
		// in the event a tuple contains another tuple, access looks something like this:
		//
		// let m = tup.0.1
		// this, unfortunately, parses as tup.(0.1)
		//
		// so, let's do this: when parsing a number, return a string. Number AST node also should store a string now.
		// string remains an accurate representation, for later merging.

		/*
			eg:

			1.0e4 == 10000
			parses as MemberAccess [ Number("1"), Number("0e4") ]

			3.004151
			parses as MemberAccess [ Number("3"), Number("004151") ]

			at typecheck time, we just join the two together, add a "." in the middle, and call stold() or something.
			storing strings preserves things like the 0s required for actually knowing what the number is.
		*/
		else if(isdigit(stream[0]) || shouldConsiderUnaryLiteral(stream, pos))
		{
			bool neg = false;
			if(stream.find("-") == 0)
				neg = true, stream = stream.substr(1);

			else if(stream.find("+") == 0)
				stream = stream.substr(1);


			int base = 10;
			if(stream.find("0x") == 0 || stream.find("0X") == 0)
				base = 16, stream = stream.substr(2);

			else if(stream.find("0b") == 0 || stream.find("0B") == 0)
				base = 2, stream = stream.substr(2);


			// find that shit (cool, we can just pass `isdigit` directly)
			auto end = std::find_if_not(stream.begin(), stream.end(), [base](const char& c) -> bool {
				if(base == 10)	return isdigit(c);
				if(base == 16)	return isdigit(c) || (toupper(c) >= 'A' && toupper(c) <= 'F');
				else			return (c == '0' || c == '1');
			});

			auto num = std::string(stream.begin(), end);
			stream = stream.substr(num.length());

			// check if we have 'e' or 'E'
			if(stream.size() > 0 && (stream[0] == 'e' || stream[0] == 'E'))
			{
				if(base != 10)
					parserError("Exponential form is supported with neither hexadecimal nor binary literals");

				// find that shit
				auto next = std::find_if_not(stream.begin() + 1, stream.end(), isdigit);

				// this adds the 'e' as well.
				num += std::string(stream.begin(), next);

				stream = stream.substr(next - stream.begin());
			}

			if(base == 16)		num = "0x" + num;
			else if(base == 2)	num = "0b" + num;

			if(neg)
				num = "-" + num;

			// we already modified stream.
			read = 0;
			tok.text = num;
			tok.type = TType::Number;
			tok.pin.len = num.length();
		}
		else if(stream[0] == '_'  || utf8iscategory(stream.data(), stream.size(), UTF8_CATEGORY_LETTER) > 0)
		{
			// get as many letters as possible first
			size_t identLength = utf8iscategory(stream.data(), stream.size(),
				UTF8_CATEGORY_LETTER | UTF8_CATEGORY_PUNCTUATION_CONNECTOR | UTF8_CATEGORY_NUMBER);

			// fprintf(stderr, "FINISH: got identifier: %s, length %zu (%zu)\n", id.c_str(), id.length(), identLength);

			bool isExclamation = (stream.size() - identLength > 0) && stream.substr(identLength).front() == '!';


			// int tmp = 0;
			// while(tmp = str.get(), (isascii(tmp) && (isalnum(tmp) || tmp == '_')) || (!isascii(tmp) && utf8is))
			// 	id += (char) tmp;

			read = identLength;
			tok.text = stream.substr(0, identLength).to_string();


			// check for keywords
			if(tok.text == "class")						tok.type = TType::Class;
			else if(tok.text == "struct")				tok.type = TType::Struct;
			else if(tok.text == FUNC_KEYWORD_STRING)	tok.type = TType::Func;
			else if(tok.text == "import")				tok.type = TType::Import;
			else if(tok.text == "var")					tok.type = TType::Var;
			else if(tok.text == "val")					tok.type = TType::Val;
			else if(tok.text == "let")					tok.type = TType::Val;
			else if(tok.text == "for")					tok.type = TType::For;
			else if(tok.text == "while")				tok.type = TType::While;
			else if(tok.text == "if")					tok.type = TType::If;
			else if(tok.text == "else")					tok.type = TType::Else;
			else if(tok.text == "return")				tok.type = TType::Return;
			else if(tok.text == "as")					{ tok.type = TType::As; if(isExclamation) { read++; tok.text = "as!"; } }
			else if(tok.text == "is")					tok.type = TType::Is;
			else if(tok.text == "switch")				tok.type = TType::Switch;
			else if(tok.text == "case")					tok.type = TType::Case;
			else if(tok.text == "enum")					tok.type = TType::Enum;
			else if(tok.text == "ffi")					tok.type = TType::ForeignFunc;
			else if(tok.text == "true")					tok.type = TType::True;
			else if(tok.text == "false")				tok.type = TType::False;
			else if(tok.text == "static")				tok.type = TType::Static;

			else if(tok.text == "break")				tok.type = TType::Break;
			else if(tok.text == "continue")				tok.type = TType::Continue;
			else if(tok.text == "do")					tok.type = TType::Do;
			else if(tok.text == "loop")					tok.type = TType::Loop;
			else if(tok.text == "defer")				tok.type = TType::Defer;

			else if(tok.text == "public")				tok.type = TType::Public;
			else if(tok.text == "private")				tok.type = TType::Private;
			else if(tok.text == "internal")				tok.type = TType::Internal;

			else if(tok.text == "alloc")				tok.type = TType::Alloc;
			else if(tok.text == "dealloc")				tok.type = TType::Dealloc;
			else if(tok.text == "typeof")				tok.type = TType::Typeof;

			else if(tok.text == "get")					tok.type = TType::Get;
			else if(tok.text == "set")					tok.type = TType::Set;

			else if(tok.text == "null")					tok.type = TType::Null;

			else if(tok.text == "module")				tok.type = TType::Module;
			else if(tok.text == "namespace")			tok.type = TType::Namespace;
			else if(tok.text == "extension")			tok.type = TType::Extension;
			else if(tok.text == "typealias")			tok.type = TType::TypeAlias;
			else if(tok.text == "protocol")				tok.type = TType::Protocol;
			else if(tok.text == "override")				tok.type = TType::Override;
			else										tok.type = TType::Identifier;
		}
		else if(stream[0] == '"')
		{
			// parse a string literal
			std::stringstream ss;

			unsigned long i = 1;
			for(; stream[i] != '"'; i++)
			{
				if(stream[i] == '\\')
				{
					i++;
					switch(stream[i])
					{
						case 'n':	ss << "\n";	break;
						case 'b':	ss << "\b";	break;
						case 'r':	ss << "\r";	break;
						case 't':	ss << "\t";	break;
						case '\\':	ss << "\\"; break;
					}

					continue;
				}

				ss << stream[i];
				if(i == stream.size() - 1 || stream[i] == '\n')
					parserError(tok, "Expected closing '\"'");
			}

			tok.type = TType::StringLiteral;
			tok.text = ss.str();
			read = i + 1;
		}
		else
		{
			if(isascii(stream[0]))
			{
				// check the first char
				switch(stream[0])
				{
					// for single-char things
					case '\n':	tok.type = TType::NewLine;	pos.line++;	break;
					case '{':	tok.type = TType::LBrace;				break;
					case '}':	tok.type = TType::RBrace;				break;
					case '(':	tok.type = TType::LParen;				break;
					case ')':	tok.type = TType::RParen;				break;
					case '[':	tok.type = TType::LSquare;				break;
					case ']':	tok.type = TType::RSquare;				break;
					case '<':	tok.type = TType::LAngle;				break;
					case '>':	tok.type = TType::RAngle;				break;
					case '+':	tok.type = TType::Plus;					break;
					case '-':	tok.type = TType::Minus;				break;
					case '*':	tok.type = TType::Asterisk;				break;
					case '/':	tok.type = TType::Divide;				break;
					case '\'':	tok.type = TType::SQuote;				break;
					case '.':	tok.type = TType::Period;				break;
					case ',':	tok.type = TType::Comma;				break;
					case ':':	tok.type = TType::Colon;				break;
					case '=':	tok.type = TType::Equal;				break;
					case '?':	tok.type = TType::Question;				break;
					case '!':	tok.type = TType::Exclamation;			break;
					case ';':	tok.type = TType::Semicolon;			break;
					case '&':	tok.type = TType::Ampersand;			break;
					case '%':	tok.type = TType::Percent;				break;
					case '|':	tok.type = TType::Pipe;					break;
					case '@':	tok.type = TType::At;					break;
					case '#':	tok.type = TType::Pound;				break;
					case '~':	tok.type = TType::Tilde;				break;
					case '^':	tok.type = TType::Caret;				break;

					default:
						parserError(tok, "Unknown token '%c'", stream[0]);
				}

				// tok.text = std::experimental::string_view(stream.begin(), 1);
				tok.text = stream[0];
				read = 1;
			}
			else if(utf8iscategory(stream.data(), stream.size(), UTF8_CATEGORY_SYMBOL_MATH | UTF8_CATEGORY_PUNCTUATION_OTHER) > 0)
			{
				read = utf8iscategory(stream.data(), stream.size(), UTF8_CATEGORY_SYMBOL_MATH | UTF8_CATEGORY_PUNCTUATION_OTHER);

				tok.text = (std::string) stream.substr(0, read);
				tok.type = TType::UnicodeSymbol;
			}
			else
			{
				parserError(tok, "Unknown token '%s'", stream.substr(0, 10).to_string().c_str());
			}
		}

		stream.remove_prefix(read);
		if(tok.type != TType::NewLine)
		{
			if(read > 0)
			{
				// note(debatable): put the actual "position" in the front of the token
				pos.col += read;

				// special handling -- things like ƒ, ≤ etc. are one character wide, but can be several *bytes* long.
				pos.len = (unicodeLength > 0 ? unicodeLength : read);
				tok.pin.len = read;
			}
		}
		else
		{
			tok.pin.col = 1;
			pos.col = 1;
		}

		previousToken = tok;
		return tok;
	}
}







