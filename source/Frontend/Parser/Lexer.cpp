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
			&& previousToken.type != TType::Integer && previousToken.type != TType::Decimal));

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


	// warning: messy function
	// edit: haha, if you think this is messy, welcome to the *REAL WORLD*

	Token getNextToken(std::experimental::string_view& stream, Pin& pos)
	{
		if(stream.length() == 0)
			return Token();

		size_t read = 0;
		size_t unicodeLength = 0;

		// first eat all whitespace
		skipWhitespace(stream, pos);

		Token tok;
		tok.pin = pos;

		// check compound symbols first.
		if(stream.compare(0, 2, "==") == 0)
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
		else if(stream.compare(0, 2, "//") == 0)
		{
			tok.text = "//";

			size_t newline = stream.find('\n');
			tok.text = (std::string) stream.substr(0, newline);
			read = tok.text.length();
			// pos.line++;

			tok.type = TType::Comment;
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
		else if(isdigit(stream[0]) || shouldConsiderUnaryLiteral(stream, pos))
		{
			std::string num;

			// read until whitespace
			std::stringstream str;
			str << stream;

			if(stream[0] == '+' || stream[0] == '-')
				num = str.get();

			int tmp = 0;
			while(isdigit(tmp = str.get()))
				num += (char) tmp;

			int base = 10;
			if(num == "0" && (tmp == 'x' || tmp == 'X' || tmp == 'b' || tmp == 'B'))
			{
				if(tmp == 'x' || tmp == 'X')
					base = 16;

				else if(tmp == 'b' || tmp == 'B')
					base = 2;

				num = "";

				while(tmp = str.get(), (base == 16 ? isxdigit(tmp) : (tmp == '0' || tmp == '1')))
					num += (char) tmp;
			}


			if(tmp == '.')
			{
				num += '.';

				if(base == 2)
					parserError(tok, "Binary floating-point literals are currently unsupported.");

				else if(base == 16)
					parserError(tok, "Hexadecimal floating-point literals are currently unsupported.");

				while(isdigit(tmp = str.get()))
					num += (char) tmp;


				// exponent form
				if(tmp == 'e' || tmp == 'E')
				{
					num += "e";
					while(isdigit(tmp = str.get()))
						num += (char) tmp;
				}


				if(num.back() == '.')
				{
					// what follow after '.' are not digits
					// pretend like the '.' was never seen, and let the parser handle this
					// can come into play when tuple members are structs, eg. tupl.0.length()
					num.pop_back();
					tok.type = TType::Integer;
				}
				else
				{
					tok.type = TType::Decimal;

					try
					{
						// makes sure we get the right shit done
						std::stold(num);

						// check if we might run out of precision
						if(num.find(".") != std::string::npos && num.substr(num.find(".")).length() > __LDBL_DIG__)
						{
							parserMessage(Err::Warn, tok, "Floating-point constant is most likely too precise for best "
								"representation (f80); have %zu digits, LDBL_DIG is %d", num.substr(num.find(".")).length(), __LDBL_DIG__);
						}
					}
					catch(const std::out_of_range&)
					{
						parserError(tok, "Number '%s' is out of range of largest representation (f80)", num.c_str());
					}
					catch(const std::exception&)
					{
						parserError(tok, "Invalid number '%s'", num.c_str());
					}
				}
			}
			else
			{
				// exponent form
				if(tmp == 'e' || tmp == 'E')
				{
					num += "e";
					while(isdigit(tmp = str.get()))
						num += (char) tmp;
				}

				// we're an int, set shit and return
				tok.type = TType::Integer;
				try
				{
					// makes sure we get the right shit done
					if(num[0] == '-')
						std::stoll(num, nullptr, base);

					else
						std::stoull(num, nullptr, base);
				}
				catch(const std::out_of_range&)
				{
					parserError(tok, "Number '%s' is out of range of largest representation (u64)", num.c_str());
				}
				catch(const std::exception&)
				{
					parserError(tok, "Invalid number '%s'", num.c_str());
				}

				if(base == 16)
					num = "0x" + num;

				else if(base == 2)
					num = "0b" + num;
			}





			// make sure the next char is not a letter, prevents things like
			// 98091824097foobar from working when 'foobar' is a var name
			// hack below to let us see the next letter without stringstream eating the space
			stream = stream.substr(num.length());

			if(stream.length() > 0 && isalpha(stream[0]))
				parserError(tok, "Malformed integer literal");

			read = 0;		// done above
			tok.text = num;
			tok.pin.len = num.length();
		}

		else if(stream[0] == '_'  || utf8iscategory(stream.data(), stream.size(), UTF8_CATEGORY_LETTER) > 0)
		{
			std::string id;

			// get as many letters as possible first
			size_t identLength = utf8iscategory(stream.data(), stream.size(),
				UTF8_CATEGORY_LETTER | UTF8_CATEGORY_PUNCTUATION_CONNECTOR | UTF8_CATEGORY_NUMBER);

			id += (std::string) stream.substr(0, identLength);

			// fprintf(stderr, "FINISH: got identifier: %s, length %zu (%zu)\n", id.c_str(), id.length(), identLength);

			bool isExclamation = (stream.size() - identLength > 0) && stream.substr(identLength).front() == '!';


			// int tmp = 0;
			// while(tmp = str.get(), (isascii(tmp) && (isalnum(tmp) || tmp == '_')) || (!isascii(tmp) && utf8is))
			// 	id += (char) tmp;

			read = id.length();
			tok.text = id;




			// check for keywords
			if(id == "class")					tok.type = TType::Class;
			else if(id == "struct")				tok.type = TType::Struct;
			else if(id == FUNC_KEYWORD_STRING)	tok.type = TType::Func;
			else if(id == "import")				tok.type = TType::Import;
			else if(id == "var")				tok.type = TType::Var;
			else if(id == "val")				tok.type = TType::Val;
			else if(id == "let")				tok.type = TType::Val;
			else if(id == "for")				tok.type = TType::For;
			else if(id == "while")				tok.type = TType::While;
			else if(id == "if")					tok.type = TType::If;
			else if(id == "else")				tok.type = TType::Else;
			else if(id == "return")				tok.type = TType::Return;
			else if(id == "as")					{ tok.type = TType::As; if(isExclamation) { read++; tok.text = "as!"; } }
			else if(id == "is")					tok.type = TType::Is;
			else if(id == "switch")				tok.type = TType::Switch;
			else if(id == "case")				tok.type = TType::Case;
			else if(id == "enum")				tok.type = TType::Enum;
			else if(id == "ffi")				tok.type = TType::ForeignFunc;
			else if(id == "true")				tok.type = TType::True;
			else if(id == "false")				tok.type = TType::False;
			else if(id == "static")				tok.type = TType::Static;

			else if(id == "break")				tok.type = TType::Break;
			else if(id == "continue")			tok.type = TType::Continue;
			else if(id == "do")					tok.type = TType::Do;
			else if(id == "loop")				tok.type = TType::Loop;
			else if(id == "defer")				tok.type = TType::Defer;

			else if(id == "public")				tok.type = TType::Public;
			else if(id == "private")			tok.type = TType::Private;
			else if(id == "internal")			tok.type = TType::Internal;

			else if(id == "alloc")				tok.type = TType::Alloc;
			else if(id == "dealloc")			tok.type = TType::Dealloc;
			else if(id == "typeof")				tok.type = TType::Typeof;

			else if(id == "get")				tok.type = TType::Get;
			else if(id == "set")				tok.type = TType::Set;

			else if(id == "null")				tok.type = TType::Null;

			else if(id == "module")				tok.type = TType::Module;
			else if(id == "namespace")			tok.type = TType::Namespace;
			else if(id == "extension")			tok.type = TType::Extension;
			else if(id == "typealias")			tok.type = TType::TypeAlias;
			else if(id == "protocol")			tok.type = TType::Protocol;
			else if(id == "override")			tok.type = TType::Override;
			else								tok.type = TType::Identifier;
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
			tok.text = "_" + ss.str();			// HACK: Parser checks for string length > 0, so if we have an empty string we
												// need something here.
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

		stream = stream.substr(read);
		if(tok.type != TType::NewLine)
		{
			// note(debatable): put the actual "position" in the front of the token
			pos.col += read;

			if(read > 0)
			{
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







