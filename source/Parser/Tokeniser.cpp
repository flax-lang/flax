// Tokeniser.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include <string>
#include <ctype.h>
#include <cassert>
#include <iostream>
#include "../include/parser.h"

namespace Parser
{
	void skipWhitespace(std::string& line)
	{
		size_t startpos = line.find_first_not_of(" \t");
		if(std::string::npos != startpos)
			line = line.substr(startpos);
	}


	// warning: messy function
	Token* getNextToken(std::string& stream, PosInfo& pos)
	{
		if(stream.length() == 0)
			return nullptr;

		int read = 0;

		// first eat all whitespace
		skipWhitespace(stream);

		Token* ret = new Token();

		Token& tok = *ret;			// because doing '->' gets old
		tok.posinfo = pos;

		// check compound symbols first.
		if(stream.find("==") == 0)
		{
			tok.text = "==";
			tok.type = TType::EqualsTo;
			read = 2;
		}
		else if(stream.find(">=") == 0)
		{
			tok.text = ">=";
			tok.type = TType::GreaterEquals;
			read = 2;
		}
		else if(stream.find("<=") == 0)
		{
			tok.text = "<=";
			tok.type = TType::LessThanEquals;
			read = 2;
		}
		else if(stream.find("!=") == 0)
		{
			tok.text = "!=";
			tok.type = TType::NotEquals;
			read = 2;
		}
		else if(stream.find("||") == 0)
		{
			tok.text = "||";
			tok.type = TType::LogicalOr;
			read = 2;
		}
		else if(stream.find("&&") == 0)
		{
			tok.text = "&&";
			tok.type = TType::LogicalAnd;
			read = 2;
		}
		else if(stream.find("->") == 0)
		{
			tok.text = "->";
			tok.type = TType::Arrow;
			read = 2;
		}
		else if(stream.find("//") == 0)
		{
			tok.text = "//";

			std::stringstream ss(stream);
			std::getline(ss, tok.text, '\n');
			read = tok.text.length();
			// pos.line++;

			tok.type = TType::Comment;
		}
		else if(stream.find("<<") == 0)
		{
			tok.text = "<<";
			tok.type = TType::ShiftLeft;
			read = 2;
		}
		else if(stream.find(">>") == 0)
		{
			tok.text = ">>";
			tok.type = TType::ShiftRight;
		}
		else if(stream.find("++") == 0)
		{
			tok.text = "++";
			tok.type = TType::DoublePlus;
			read = 2;
		}
		else if(stream.find("--") == 0)
		{
			tok.text = "--";
			tok.type = TType::DoubleMinus;
			read = 2;
		}
		else if(stream.find("+=") == 0)
		{
			tok.text = "+=";
			tok.type = TType::PlusEq;
			read = 2;
		}
		else if(stream.find("-=") == 0)
		{
			tok.text = "+=";
			tok.type = TType::MinusEq;
			read = 2;
		}
		else if(stream.find("*=") == 0)
		{
			tok.text = "+=";
			tok.type = TType::MultiplyEq;
			read = 2;
		}
		else if(stream.find("/=") == 0)
		{
			tok.text = "+=";
			tok.type = TType::DivideEq;
			read = 2;
		}
		else if(stream.find("%=") == 0)
		{
			tok.text = "%=";
			tok.type = TType::ModEq;
			read = 2;
		}
		else if(stream.find("<<=") == 0)
		{
			tok.text = "<<=";
			tok.type = TType::ShiftLeftEq;
			read = 3;
		}
		else if(stream.find(">>=") == 0)
		{
			tok.text = ">>=";
			tok.type = TType::ShiftRightEq;
			read = 3;
		}
		else if(stream.find("...") == 0)
		{
			tok.text = "...";
			tok.type = TType::Elipsis;
			read = 3;
		}
		else if(isdigit(stream[0]))
		{
			// todo: handle hex

			std::string num;

			// read until whitespace
			std::stringstream str;
			str << stream;

			int tmp = 0;
			while(isdigit(tmp = str.get()))
				num += (char) tmp;

			int base = 10;
			if(num == "0" && tmp == 'x')
			{
				base = 16;
				num = "";

				while(isdigit(tmp = str.get()))
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
					Parser::parserError("Invalid number\n");
					exit(1);
				}

				if(base == 16)
					num = "0x" + num;
			}
			else if(base == 10)
			{
				num += '.';

				while(isdigit(tmp = str.get()))
					num += (char) tmp;

				tok.type = TType::Decimal;

				try
				{
					// makes sure we get the right shit done
					std::stod(num);
				}
				catch(std::exception)
				{
					Parser::parserError("Invalid decimal number\n");
					exit(1);
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
		}
		else if(isalpha(stream[0]) || stream[0] == '_')
		{
			std::string id;

			// read until whitespace
			std::stringstream str;
			str << stream;


			int tmp = 0;
			while(tmp = str.get(), (isalnum(tmp) || tmp == '_'))
				id += (char) tmp;


			read = id.length();
			tok.text = id;


			// check for keywords
			if(id == "class")			tok.type = TType::Class;
			else if(id == "func")		tok.type = TType::Func;
			else if(id == "import")		tok.type = TType::Import;
			else if(id == "var")		tok.type = TType::Var;
			else if(id == "val")		tok.type = TType::Val;
			// else if(id == "ptr")		tok.type = TType::Ptr;
			// else if(id == "deref")		tok.type = TType::Deref;
			// else if(id == "addrof")		tok.type = TType::Addr;
			else if(id == "for")		tok.type = TType::For;
			else if(id == "while")		tok.type = TType::While;
			else if(id == "if")			tok.type = TType::If;
			else if(id == "else")		tok.type = TType::Else;
			else if(id == "return")		tok.type = TType::Return;
			else if(id == "as")			tok.type = TType::As;
			else if(id == "is")			tok.type = TType::Is;
			else if(id == "switch")		tok.type = TType::Switch;
			else if(id == "case")		tok.type = TType::Case;
			else if(id == "enum")		tok.type = TType::Enum;
			else if(id == "ffi")		tok.type = TType::ForeignFunc;
			else if(id == "struct")		tok.type = TType::Struct;
			else if(id == "true")		tok.type = TType::True;
			else if(id == "false")		tok.type = TType::False;

			else if(id == "break")		tok.type = TType::Break;
			else if(id == "continue")	tok.type = TType::Continue;
			else if(id == "do")			tok.type = TType::Do;

			else if(id == "public")		tok.type = TType::Public;
			else if(id == "private")	tok.type = TType::Private;
			else if(id == "internal")	tok.type = TType::Internal;

			else if(id == "Int8"
				|| id == "Int16"
				|| id == "Int32"
				|| id == "Int64"
				|| id == "Uint8"
				|| id == "Uint16"
				|| id == "Uint32"
				|| id == "Uint64"
				|| id == "Float32"
				|| id == "Float64"
				|| id == "AnyPtr"
				|| id == "Bool"
				|| id == "Void")		tok.type = TType::BuiltinType;

			else						tok.type = TType::Identifier;
		}
		else if(stream[0] == '"')
		{
			// parse a string literal
			std::stringstream ss;

			int i = 1;
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
				if(i == stream.size() - 1)
					Parser::parserError("Expected closing '\"'");
			}

			tok.type = TType::StringLiteral;
			tok.text = ss.str();
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
			delete ret;
			Parser::parserError("Unknown token '%c'", stream[0]);
		}

		stream = stream.substr(read);
		return ret;
	}
}







