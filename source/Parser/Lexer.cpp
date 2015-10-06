// Lexer.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include <string>
#include <ctype.h>
#include <cassert>
#include <iostream>
#include "../include/parser.h"

namespace Parser
{
	static void skipWhitespace(std::string& line, pin& pos)
	{
		size_t startpos = line.find_first_not_of(" \t");
		if(startpos != std::string::npos)
		{
			pos.col += startpos;
			line = line.substr(startpos);
		}
	}


	// warning: messy function
	Token getNextToken(std::string& stream, pin& pos)
	{
		if(stream.length() == 0)
			return Token();

		int read = 0;

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

			std::stringstream ss(stream);
			std::getline(ss, tok.text, '\n');
			read = tok.text.length();
			// pos.line++;

			tok.type = TType::Comment;
		}
		else if(stream.compare(0, 2, "<<") == 0)
		{
			tok.text = "<<";
			tok.type = TType::ShiftLeft;
			read = 2;
		}
		else if(stream.compare(0, 2, ">>") == 0)
		{
			tok.text = ">>";
			tok.type = TType::ShiftRight;
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
		else if(stream.compare(0, 3, "<<=") == 0)
		{
			tok.text = "<<=";
			tok.type = TType::ShiftLeftEq;
			read = 3;
		}
		else if(stream.compare(0, 3, ">>=") == 0)
		{
			tok.text = ">>=";
			tok.type = TType::ShiftRightEq;
			read = 3;
		}
		else if(stream.compare(0, 3, "...") == 0)
		{
			tok.text = "...";
			tok.type = TType::Elipsis;
			read = 3;
		}
		else if(stream.compare(0, 2, "::") == 0)
		{
			tok.text = "::";
			tok.type = TType::DoubleColon;
			read = 2;
		}
		// block comments
		else if(stream.compare(0, 2, "/*") == 0)
		{
			// TODO: BLOCK COMMENTS ARE FUCKING BUGGY AND IFFY

			int currentNest = 1;
			// support nested, so basically we have to loop until we find either a /* or a */
			stream = stream.substr(2);
			tok.pin.col += 2;

			while(currentNest > 0)
			{
				size_t n = stream.find("/*");
				if(n != std::string::npos)
				{
					std::string removed = stream.substr(0, n);

					tok.pin.line += std::count(removed.begin(), removed.end(), '\n');
					tok.pin.col += removed.length() - removed.find_last_of("\n");

					stream = stream.substr(n + 2);	// include the '*' as well.

					if(currentNest > 1)
						currentNest++;
				}



				n = stream.find("*/");
				if(n != std::string::npos)
				{
					std::string removed = stream.substr(0, n);

					tok.pin.line += std::count(removed.begin(), removed.end(), '\n');
					tok.pin.col += removed.length() - removed.find_last_of("\n");

					stream = stream.substr(n + 2);	// include the '*' as well.

					currentNest--;
				}
				else
				{
					parserError("Expected closing '*/'");
				}
			}

			return getNextToken(stream, pos);
		}
		else if(stream.compare(0, 2, "*/") == 0)
		{
			parserError("Unexpected '*/'");
		}

		// unicode stuff
		else if(stream.compare(0, strlen("ƒ"), "ƒ") == 0)
		{
			tok.text = "func";
			tok.type = TType::Func;
			read = std::string("ƒ").length();
		}
		else if(stream.compare(0, strlen("ﬁ"), "ﬁ") == 0)
		{
			tok.text = "ffi";
			tok.type = TType::ForeignFunc;
			read = std::string("ﬁ").length();
		}
		else if(stream.compare(0, strlen("÷"), "÷") == 0)
		{
			tok.text = "÷";
			tok.type = TType::Divide;
			read = std::string("÷").length();
		}
		else if(stream.compare(0, strlen("≠"), "≠") == 0)
		{
			tok.text = "≠";
			tok.type = TType::NotEquals;
			read = std::string("≠").length();
		}
		else if(stream.compare(0, strlen("≤"), "≤") == 0)
		{
			tok.text = "≤";
			tok.type = TType::LessThanEquals;
			read = std::string("≤").length();
		}
		else if(stream.compare(0, strlen("≥"), "≥") == 0)
		{
			tok.text = "≥";
			tok.type = TType::GreaterEquals;
			read = std::string("≥").length();
		}
		else if(isdigit(stream[0]))
		{
			std::string num;

			// read until whitespace
			std::stringstream str;
			str << stream;

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

			if(tmp != '.')
			{
				// we're an int, set shit and return
				tok.type = TType::Integer;
				try
				{
					// makes sure we get the right shit done
					std::stoll(num, nullptr, base);
				}
				catch(std::exception)
				{
					Parser::parserError("Invalid number '%s'\n", num.c_str());
				}

				if(base == 16)
					num = "0x" + num;

				else if(base == 2)
					num = "0b" + num;
			}
			else if(base == 10)
			{
				num += '.';

				while(isdigit(tmp = str.get()))
					num += (char) tmp;

				if(num.back() == '.')
				{
					if(isalpha(tmp))
					{
						tok.type = TType::Integer;
						num = num.substr(0, num.length() - 1);
						str.put('.');
					}
					else
					{
						Parser::parserError("Expected more numbers after '.'");
					}
				}
				else
				{
					tok.type = TType::Decimal;

					try
					{
						// makes sure we get the right shit done
						std::stod(num);
					}
					catch(std::exception)
					{
						Parser::parserError("Invalid number");
					}
				}
			}
			else
			{
				Parser::parserError("Decimals in hexadecimal representation are not supported");
			}


			// make sure the next char is not a letter, prevents things like
			// 98091824097foobar from working when 'foobar' is a var name
			// hack below to let us see the next letter without stringstream eating the space
			stream = stream.substr(num.length());

			if(stream.length() > 0 && isalpha(stream[0]))
				Parser::parserError("Malformed integer literal");

			read = 0;		// done above
			tok.text = num;
			pos.len = num.length();
		}
		else if(isalpha(stream[0]) || stream[0] == '_' || !isascii(stream[0]))
		{
			std::string id;

			// read until whitespace
			std::stringstream str;
			str << stream;


			int tmp = 0;
			while(tmp = str.get(), (isascii(tmp) && (isalnum(tmp) || tmp == '_')) || !isascii(tmp))
				id += (char) tmp;


			read = id.length();
			tok.text = id;


			// check for keywords
			if(id == "class")			tok.type = TType::Class;
			else if(id == "struct")		tok.type = TType::Struct;
			else if(id == "func")		tok.type = TType::Func;
			else if(id == "import")		tok.type = TType::Import;
			else if(id == "var")		tok.type = TType::Var;
			else if(id == "val")		tok.type = TType::Val;
			else if(id == "let")		tok.type = TType::Val;
			else if(id == "for")		tok.type = TType::For;
			else if(id == "while")		tok.type = TType::While;
			else if(id == "if")			tok.type = TType::If;
			else if(id == "else")		tok.type = TType::Else;
			else if(id == "return")		tok.type = TType::Return;
			else if(id == "as")			{ tok.type = TType::As; if(tmp == '!') { read++; tok.text = "as!"; } }
			else if(id == "is")			tok.type = TType::Is;
			else if(id == "switch")		tok.type = TType::Switch;
			else if(id == "case")		tok.type = TType::Case;
			else if(id == "enum")		tok.type = TType::Enum;
			else if(id == "ffi")		tok.type = TType::ForeignFunc;
			else if(id == "true")		tok.type = TType::True;
			else if(id == "false")		tok.type = TType::False;
			else if(id == "static")		tok.type = TType::Static;

			else if(id == "break")		tok.type = TType::Break;
			else if(id == "continue")	tok.type = TType::Continue;
			else if(id == "do")			tok.type = TType::Do;
			else if(id == "loop")		tok.type = TType::Loop;
			else if(id == "defer")		tok.type = TType::Defer;

			else if(id == "public")		tok.type = TType::Public;
			else if(id == "private")	tok.type = TType::Private;
			else if(id == "internal")	tok.type = TType::Internal;

			else if(id == "alloc")		tok.type = TType::Alloc;
			else if(id == "dealloc")	tok.type = TType::Dealloc;
			else if(id == "typeof")		tok.type = TType::Typeof;

			else if(id == "get")		tok.type = TType::Get;
			else if(id == "set")		tok.type = TType::Set;

			else if(id == "module")		tok.type = TType::Module;
			else if(id == "namespace")	tok.type = TType::Namespace;
			else if(id == "extension")	tok.type = TType::Extension;
			else if(id == "typealias")	tok.type = TType::TypeAlias;
			else if(id == "override")	tok.type = TType::Override;
			else						tok.type = TType::Identifier;
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
						case '\\':	ss << "\\"; break;
					}

					continue;
				}

				ss << stream[i];
				if(i == stream.size() - 1 || stream[i] == '\n')
					Parser::parserError("Expected closing '\"'");
			}

			tok.type = TType::StringLiteral;
			tok.text = "_" + ss.str();			// HACK: Parser checks for string length > 0, so if we have an empty string we
												// need something here.
			read = i + 1;
		}
		else if(!isalnum(stream[0]))
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
			}

			tok.text = stream[0];
			read = 1;
		}
		else
		{
			// delete ret;
			Parser::parserError("Unknown token '%c'", stream[0]);
		}

		stream = stream.substr(read);
		if(tok.type != TType::NewLine)
		{
			tok.pin.col += read;
			pos.col += read;

			if(read > 0)
				pos.len = read;
		}
		else
		{
			tok.pin.col = 1;
			pos.col = 1;
		}

		return tok;
	}
}







