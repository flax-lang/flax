// Tokeniser.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include <string>
#include <iostream>
#include <cassert>
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
		tok.posinfo = new PosInfo(pos);

		// check compounds first.
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
		else if(stream.find("->") == 0)
		{
			tok.text = "->";
			tok.type = TType::Arrow;
			read = 2;
		}
		else if(stream.find("//") == 0)
		{
			tok.text = "//";

			std::stringstream ss;
			std::getline(ss, tok.text, '\n');
			read = tok.text.length();
			pos.line++;

			tok.type = TType::Comment;
		}
		else if(stream.find("<<") == 0)
		{
			tok.text = "<<";
			tok.type = TType::ShiftLeft;
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
		}
		else if(stream.find("--") == 0)
		{
			tok.text = "--";
			tok.type = TType::DoubleMinus;
		}
		else if(stream.find("+=") == 0)
		{
			tok.text = "+=";
			tok.type = TType::PlusEq;
		}
		else if(stream.find("-=") == 0)
		{
			tok.text = "+=";
			tok.type = TType::MinusEq;
		}
		else if(stream.find("*=") == 0)
		{
			tok.text = "+=";
			tok.type = TType::MultiplyEq;
		}
		else if(stream.find("/=") == 0)
		{
			tok.text = "+=";
			tok.type = TType::DivideEq;
		}
		else if(stream.find("%=") == 0)
		{
			tok.text = "%=";
			tok.type = TType::ModEq;
		}
		else if(stream.find("<<=") == 0)
		{
			tok.text = "<<=";
			tok.type = TType::ShiftLeftEq;
		}
		else if(stream.find(">>=") == 0)
		{
			tok.text = ">>=";
			tok.type = TType::ShiftRightEq;
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
				case '"':	tok.type = TType::DQuote;				break;
				case '.':	tok.type = TType::Period;				break;
				case ',':	tok.type = TType::Comma;				break;
				case ':':	tok.type = TType::Colon;				break;
				case '=':	tok.type = TType::Equal;				break;
				case '?':	tok.type = TType::Question;				break;
				case '!':	tok.type = TType::Exclamation;			break;
				case ';':	tok.type = TType::Semicolon;			break;
				case '&':	tok.type = TType::Ampersand;			break;
				case '%':	tok.type = TType::Percent;				break;
			}

			tok.text = stream[0];
			read = 1;
		}
		else if(isnumber(stream[0]))
		{
			// todo: handle hex

			std::string num;

			// read until whitespace
			std::stringstream str;
			str << stream;

			int tmp = 0;
			while(isnumber(tmp = str.get()))
				num += (char) tmp;

			if(tmp != '.')
			{
				// we're an int, set shit and return
				tok.type = TType::Integer;

				try
				{
					// makes sure we get the right shit done
					std::stoll(num);
				}
				catch(std::exception)
				{
					fprintf(stderr, "Error: invalid number found at (%s:%lld)\n", pos.file->c_str(), pos.line);
					exit(1);
				}
			}
			else
			{
				num += '.';

				while(isnumber(tmp = str.get()))
					num += (char) tmp;

				tok.type = TType::Decimal;


				try
				{
					// makes sure we get the right shit done
					std::stod(num);
				}
				catch(std::exception)
				{
					fprintf(stderr, "Error: invalid decimal found at (%s:%lld)\n", pos.file->c_str(), pos.line);
					exit(1);
				}
			}

			read = num.length();
			tok.text = num;
		}
		else if(isalpha(stream[0]))
		{
			std::string id;

			// read until whitespace
			std::stringstream str;
			str << stream;


			int tmp = 0;
			while(isalnum(tmp = str.get()))
				id += (char) tmp;


			read = id.length();
			tok.text = id;


			// check for keywords
			// TODO: there has to be a better way
			if(id == "class")			tok.type = TType::Class;
			else if(id == "func")		tok.type = TType::Func;
			else if(id == "import")		tok.type = TType::Import;
			else if(id == "var")		tok.type = TType::Var;
			else if(id == "val")		tok.type = TType::Val;
			else if(id == "ptr")		tok.type = TType::Ptr;
			else if(id == "deref")		tok.type = TType::Deref;
			else if(id == "addr")		tok.type = TType::Addr;
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

			else if(id == "public")		tok.type = TType::Public;
			else if(id == "private")	tok.type = TType::Private;
			else if(id == "internal")	tok.type = TType::Internal;

			else						tok.type = TType::Identifier;
		}
		else
		{
			printf("Unknown token '%c' at (%s:%lld)\n", stream[0], pos.file->c_str(), pos.line);

			delete ret;
			exit(1);
		}

		stream = stream.substr(read);
		return ret;
	}
}







