// Lexer.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include <string>
#include <ctype.h>
#include <cassert>
#include <iostream>

#include "parser.h"
#include "../utf8rewind/include/utf8rewind/utf8rewind.h"

using string_view = std::experimental::string_view;

namespace Lexer
{
	using namespace Parser;

	static void skipWhitespace(string_view& line, Pin& pos)
	{
		size_t skip = 0;
		while(line.length() > skip && (line[skip] == '\t' || line[skip] == ' '))
			skip++, (line[skip] == ' ' ? pos.col++ : pos.col += TAB_WIDTH);

		line.remove_prefix(skip);
	}

	template <size_t N>
	static bool hasPrefix(const string_view& str, char const (&literal)[N])
	{
		if(str.length() < N - 1) return false;
		for(size_t i = 0; i < N - 1; i++)
			if(str[i] != literal[i]) return false;

		return true;
	}

	template <size_t N>
	static bool compare(const string_view& str, char const (&literal)[N])
	{
		if(str.length() != N - 1) return false;
		for(size_t i = 0; i < N - 1; i++)
			if(str[i] != literal[i]) return false;

		return true;
	}



	static Token previousToken;
	static bool shouldConsiderUnaryLiteral(string_view& stream, Pin& pos)
	{
		// check the previous token
		bool res = (previousToken.type != TType::Invalid && previousToken.pin.fileID == pos.fileID &&
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


	Token getNextToken(std::vector<string_view>& lines, size_t* line, const string_view& whole, Pin& pos)
	{
		bool flag = true;

		if(*line == lines.size())
		{
			Token ret;
			ret.type = TType::EndOfFile;
			return ret;
		}

		string_view stream = lines[*line];

		size_t read = 0;
		size_t unicodeLength = 0;

		// first eat all whitespace
		skipWhitespace(stream, pos);

		Token tok;
		tok.pin = pos;

		// check compound symbols first.
		if(hasPrefix(stream, "//"))
		{
			// size_t newline = stream.find('\n');

			// tok.text = stream.substr(0, newline).to_string();

			// tok.text = stream.substr(0, stream.length() - 1);
			tok.type = TType::Comment;
			stream = stream.substr(0, 0);
			(*line)++;
			pos.line++;

			// don't assign lines[line] = stream, since over here we've changed 'line' to be the next one.
			flag = false;
		}
		else if(hasPrefix(stream, "=="))
		{
			tok.text = "==";
			tok.type = TType::EqualsTo;
			read = 2;
		}
		else if(hasPrefix(stream, ">="))
		{
			tok.text = ">=";
			tok.type = TType::GreaterEquals;
			read = 2;
		}
		else if(hasPrefix(stream, "<="))
		{
			tok.text = "<=";
			tok.type = TType::LessThanEquals;
			read = 2;
		}
		else if(hasPrefix(stream, "!="))
		{
			tok.text = "!=";
			tok.type = TType::NotEquals;
			read = 2;
		}
		else if(hasPrefix(stream, "||"))
		{
			tok.text = "||";
			tok.type = TType::LogicalOr;
			read = 2;
		}
		else if(hasPrefix(stream, "&&"))
		{
			tok.text = "&&";
			tok.type = TType::LogicalAnd;
			read = 2;
		}
		else if(hasPrefix(stream, "->"))
		{
			tok.text = "->";
			tok.type = TType::Arrow;
			read = 2;
		}
		else if(hasPrefix(stream, "++"))
		{
			tok.text = "++";
			tok.type = TType::DoublePlus;
			read = 2;
		}
		else if(hasPrefix(stream, "--"))
		{
			tok.text = "--";
			tok.type = TType::DoubleMinus;
			read = 2;
		}
		else if(hasPrefix(stream, "+="))
		{
			tok.text = "+=";
			tok.type = TType::PlusEq;
			read = 2;
		}
		else if(hasPrefix(stream, "-="))
		{
			tok.text = "+=";
			tok.type = TType::MinusEq;
			read = 2;
		}
		else if(hasPrefix(stream, "*="))
		{
			tok.text = "+=";
			tok.type = TType::MultiplyEq;
			read = 2;
		}
		else if(hasPrefix(stream, "/="))
		{
			tok.text = "+=";
			tok.type = TType::DivideEq;
			read = 2;
		}
		else if(hasPrefix(stream, "%="))
		{
			tok.text = "%=";
			tok.type = TType::ModEq;
			read = 2;
		}
		else if(hasPrefix(stream, "&="))
		{
			tok.text = "&=";
			tok.type = TType::AmpersandEq;
			read = 2;
		}
		else if(hasPrefix(stream, "|="))
		{
			tok.text = "|=";
			tok.type = TType::PipeEq;
			read = 2;
		}
		else if(hasPrefix(stream, "^="))
		{
			tok.text = "^=";
			tok.type = TType::CaretEq;
			read = 2;
		}
		else if(hasPrefix(stream, "..."))
		{
			tok.text = "...";
			tok.type = TType::Ellipsis;
			read = 3;
		}
		else if(hasPrefix(stream, "/*"))
		{
			int currentNest = 1;

			// support nested, so basically we have to loop until we find either a /* or a */
			stream.remove_prefix(2);

			pos.col += 2;

			Pin opening = pos;
			Pin curpos = pos;

			size_t k = 0;
			while(currentNest > 0)
			{
				// we can do this, because we know the closing token (*/) is 2 chars long
				// so if we have 1 char left, gg.
				if(k + 1 == stream.size() || stream[k] == '\n')
				{
					if(lines.size() == 1)
						parserError(opening, "Expected closing */ (reached EOF), for block comment started here:");

					// else, get the next line.
					// also note: if we're in this loop, we're inside a block comment.
					// since the ending token cannot be split across lines, we know that this last char
					// must also be part of the comment. hence, just skip over it.

					k = 0;
					curpos.line++;
					curpos.col = 0;
					(*line)++;

					stream = lines[*line];
					continue;
				}


				if(stream[k] == '/' && stream[k + 1] == '*')
					currentNest++, k++, curpos.col++, opening = curpos;

				else if(stream[k] == '*' && stream[k + 1] == '/')
					currentNest--, k++, curpos.col++;

				k++;
				curpos.col++;
			}

			if(currentNest != 0)
				parserError(opening, "Expected closing */ (reached EOF), for block comment started here:");

			pos = curpos;

			// don't actually store the text, because it's pointless and memory-wasting
			tok.text = "/* I used to be a comment like you, until I took a memory-leak to the knee. */";
			tok.type = TType::Comment;
			read = k;
		}
		else if(hasPrefix(stream, "*/"))
		{
			parserError(tok, "Unexpected '*/'");
		}

		// unicode stuff
		else if(hasPrefix(stream, "ƒ"))
		{
			tok.text = FUNC_KEYWORD_STRING;
			tok.type = TType::Func;
			read = std::string("ƒ").length();

			unicodeLength = 1;
		}
		else if(hasPrefix(stream, "ﬁ"))
		{
			tok.text = "ffi";
			tok.type = TType::ForeignFunc;
			read = std::string("ﬁ").length();

			unicodeLength = 1;
		}
		else if(hasPrefix(stream, "÷"))
		{
			tok.text = "÷";
			tok.type = TType::Divide;
			read = std::string("÷").length();

			unicodeLength = 1;
		}
		else if(hasPrefix(stream, "≠"))
		{
			tok.text = "≠";
			tok.type = TType::NotEquals;
			read = std::string("≠").length();

			unicodeLength = 1;
		}
		else if(hasPrefix(stream, "≤"))
		{
			tok.text = "≤";
			tok.type = TType::LessThanEquals;
			read = std::string("≤").length();

			unicodeLength = 1;
		}
		else if(hasPrefix(stream, "≥"))
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
		else if(!stream.empty() && (isdigit(stream[0]) || shouldConsiderUnaryLiteral(stream, pos)))
		{
			std::string prefix;

			// copy it.
			auto tmp = stream;

			if(stream.find('-') == 0 || stream.find('+') == 0)
				prefix = stream[0], tmp.remove_prefix(1);

			int base = 10;
			if(tmp.find("0x") == 0 || tmp.find("0X") == 0)
				base = 16, tmp.remove_prefix(2);

			else if(tmp.find("0b") == 0 || tmp.find("0B") == 0)
				base = 2, tmp.remove_prefix(2);


			// find that shit (cool, we can just pass `isdigit` directly)
			auto end = std::find_if_not(tmp.begin(), tmp.end(), [base](const char& c) -> bool {
				if(base == 10)	return isdigit(c);
				if(base == 16)	return isdigit(c) || (toupper(c) >= 'A' && toupper(c) <= 'F');
				else			return (c == '0' || c == '1');
			});

			auto num = std::string(tmp.begin(), end);
			tmp.remove_prefix(num.length());

			// check if we have 'e' or 'E'
			if(tmp.size() > 0 && (tmp[0] == 'e' || tmp[0] == 'E'))
			{
				if(base != 10)
					parserError("Exponential form is supported with neither hexadecimal nor binary literals");

				// find that shit
				auto next = std::find_if_not(tmp.begin() + 1, tmp.end(), isdigit);

				// this adds the 'e' as well.
				num += std::string(tmp.begin(), next);

				tmp.remove_prefix(next - tmp.begin());
			}

			if(base == 16)		num = "0x" + num;
			else if(base == 2)	num = "0b" + num;

			num = prefix + num;

			// we already modified stream.
			read = 0;
			tok.text = stream.substr(0, num.length());
			tok.type = TType::Number;
			tok.pin.len = num.length();

			stream = stream.substr(num.length());
		}
		else if(!stream.empty() && (stream[0] == '_'  || utf8iscategory(stream.data(), stream.size(), UTF8_CATEGORY_LETTER) > 0))
		{
			// get as many letters as possible first
			size_t identLength = utf8iscategory(stream.data(), stream.size(),
				UTF8_CATEGORY_LETTER | UTF8_CATEGORY_PUNCTUATION_CONNECTOR | UTF8_CATEGORY_NUMBER);

			bool isExclamation = (stream.size() - identLength > 0) && stream.substr(identLength).front() == '!';


			read = identLength;
			tok.text = stream.substr(0, identLength);


			// check for keywords
			if(compare(tok.text, "class"))					tok.type = TType::Class;
			else if(compare(tok.text, "struct"))			tok.type = TType::Struct;
			else if(compare(tok.text, FUNC_KEYWORD_STRING))	tok.type = TType::Func;
			else if(compare(tok.text, "import"))			tok.type = TType::Import;
			else if(compare(tok.text, "var"))				tok.type = TType::Var;
			else if(compare(tok.text, "val"))				tok.type = TType::Val;
			else if(compare(tok.text, "let"))				tok.type = TType::Val;
			else if(compare(tok.text, "for"))				tok.type = TType::For;
			else if(compare(tok.text, "while"))				tok.type = TType::While;
			else if(compare(tok.text, "if"))				tok.type = TType::If;
			else if(compare(tok.text, "else"))				tok.type = TType::Else;
			else if(compare(tok.text, "return"))			tok.type = TType::Return;
			else if(compare(tok.text, "is"))				tok.type = TType::Is;
			else if(compare(tok.text, "switch"))			tok.type = TType::Switch;
			else if(compare(tok.text, "case"))				tok.type = TType::Case;
			else if(compare(tok.text, "enum"))				tok.type = TType::Enum;
			else if(compare(tok.text, "ffi"))				tok.type = TType::ForeignFunc;
			else if(compare(tok.text, "true"))				tok.type = TType::True;
			else if(compare(tok.text, "false"))				tok.type = TType::False;
			else if(compare(tok.text, "static"))			tok.type = TType::Static;

			else if(compare(tok.text, "break"))				tok.type = TType::Break;
			else if(compare(tok.text, "continue"))			tok.type = TType::Continue;
			else if(compare(tok.text, "do"))				tok.type = TType::Do;
			else if(compare(tok.text, "loop"))				tok.type = TType::Loop;
			else if(compare(tok.text, "defer"))				tok.type = TType::Defer;

			else if(compare(tok.text, "public"))			tok.type = TType::Public;
			else if(compare(tok.text, "private"))			tok.type = TType::Private;
			else if(compare(tok.text, "internal"))			tok.type = TType::Internal;

			else if(compare(tok.text, "alloc"))				tok.type = TType::Alloc;
			else if(compare(tok.text, "dealloc"))			tok.type = TType::Dealloc;
			else if(compare(tok.text, "typeof"))			tok.type = TType::Typeof;

			else if(compare(tok.text, "get"))				tok.type = TType::Get;
			else if(compare(tok.text, "set"))				tok.type = TType::Set;

			else if(compare(tok.text, "null"))				tok.type = TType::Null;

			else if(compare(tok.text, "module"))			tok.type = TType::Module;
			else if(compare(tok.text, "namespace"))			tok.type = TType::Namespace;
			else if(compare(tok.text, "extension"))			tok.type = TType::Extension;
			else if(compare(tok.text, "typealias"))			tok.type = TType::TypeAlias;
			else if(compare(tok.text, "protocol"))			tok.type = TType::Protocol;
			else if(compare(tok.text, "override"))			tok.type = TType::Override;

			else if(compare(tok.text, "as"))			{ tok.type = TType::As; if(isExclamation) { read++; tok.type = TType::AsExclamation; } }
			else										tok.type = TType::Identifier;
		}
		else if(!stream.empty() && stream[0] == '"')
		{
			// string literal
			auto ss = std::stringstream();

			// because we want to avoid using std::string (ie. copying) in the lexer (Token), we must send the string over verbatim.

			// store the starting position
			size_t start = stream.begin() - whole.begin() + 1;
			size_t extra = 0;

			size_t i = 1;
			for(; stream[i] != '"'; i++)
			{
				if(stream[i] == '\\')
				{
					if(i + 1 == stream.size())
					{
						parserError("Unexpected end of input");
					}
					else if(stream[i + 1] == '"')
					{
						// add the quote and the backslash, and skip it.
						ss << "\\\"";
						i++;
					}
					// breaking string over two lines
					else if(stream[i + 1] == '\n')
					{
						// skip it, then move to the next line
						pos.line++;
						pos.col = 1;
						(*line)++;

						i = 0;

						// just a fudge factor gotten from empirical evidence
						// 3 extra holds for multiple lines, so all is well.

						extra += 3;
						stream = lines[*line];
					}
					else
					{
						// just put the backslash in.
						// and don't skip the next one.
						ss << '\\';
					}

					continue;
				}

				ss << stream[i];
				if(i == stream.size() - 1 || stream[i] == '\n')
					parserError(Pin(pos.fileID, pos.line, pos.col + i, pos.len), "Expected closing '\"'");
			}

			tok.type = TType::StringLiteral;
			tok.text = whole.substr(start, ss.str().length() + extra);
			read = i + 1;
		}
		else if(!stream.empty())
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

				tok.text = string_view(stream.begin(), 1);
				read = 1;
			}
			else if(utf8iscategory(stream.data(), stream.size(), UTF8_CATEGORY_SYMBOL_MATH | UTF8_CATEGORY_PUNCTUATION_OTHER) > 0)
			{
				read = utf8iscategory(stream.data(), stream.size(), UTF8_CATEGORY_SYMBOL_MATH | UTF8_CATEGORY_PUNCTUATION_OTHER);

				tok.text = stream.substr(0, read);
				tok.type = TType::UnicodeSymbol;
			}
			else
			{
				parserError(tok, "Unknown token '%s'", stream.substr(0, 10).to_string().c_str());
			}
		}

		stream.remove_prefix(read);

		if(flag) lines[*line] = stream;

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

			(*line)++;
		}

		previousToken = tok;
		return tok;
	}
}







