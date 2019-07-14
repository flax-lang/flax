// lexer.cpp
// Copyright (c) 2014 - 2015, zhiayang
// Licensed under the Apache License Version 2.0.

#include "lexer.h"
#include "errors.h"

#include "utf8rewind/include/utf8rewind/utf8rewind.h"

using string_view = util::string_view;

namespace lexer
{
	static void skipWhitespace(string_view& line, Location& pos, size_t* offset)
	{
		size_t skip = 0;
		while(line.length() > skip && (line[skip] == '\t' || line[skip] == ' '))
		{
			(line[skip] == ' ' ? pos.col++ : pos.col += TAB_WIDTH);
			skip++;
		}

		line.remove_prefix(skip);
		(*offset) += skip;
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



	static TokenType prevType = TokenType::Invalid;
	static size_t prevID = 0;
	static bool shouldConsiderUnaryLiteral(string_view& stream, Location& pos)
	{
		// check the previous token
		bool should = (prevType != TokenType::Invalid && prevID == pos.fileID && (
			prevType != TokenType::RParen &&
			prevType != TokenType::RSquare &&
			prevType != TokenType::Identifier &&
			prevType != TokenType::Number &&
			prevType != TokenType::Dollar &&
			prevType != TokenType::StringLiteral
		));

		if(!should) return false;

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


	static util::hash_map<util::string_view, TokenType> keywordMap;
	static void initKeywordMap()
	{
		if(keywordMap.size() > 0) return;

		keywordMap["as"]        = TokenType::As;
		keywordMap["do"]        = TokenType::Do;
		keywordMap["if"]        = TokenType::If;
		keywordMap["is"]        = TokenType::Is;
		keywordMap["let"]       = TokenType::Val;
		keywordMap["var"]       = TokenType::Var;
		keywordMap["for"]       = TokenType::For;
		keywordMap["fn"]        = TokenType::Func;
		keywordMap["else"]      = TokenType::Else;
		keywordMap["true"]      = TokenType::True;
		keywordMap["enum"]      = TokenType::Enum;
		keywordMap["null"]      = TokenType::Null;
		keywordMap["case"]      = TokenType::Case;
		keywordMap["trait"]     = TokenType::Trait;
		keywordMap["defer"]     = TokenType::Defer;
		keywordMap["alloc"]     = TokenType::Alloc;
		keywordMap["false"]     = TokenType::False;
		keywordMap["while"]     = TokenType::While;
		keywordMap["break"]     = TokenType::Break;
		keywordMap["class"]     = TokenType::Class;
		keywordMap["using"]     = TokenType::Using;
		keywordMap["union"]     = TokenType::Union;
		keywordMap["struct"]    = TokenType::Struct;
		keywordMap["import"]    = TokenType::Import;
		keywordMap["public"]    = TokenType::Public;
		keywordMap["switch"]    = TokenType::Switch;
		keywordMap["return"]    = TokenType::Return;
		keywordMap["export"]    = TokenType::Export;
		keywordMap["sizeof"]    = TokenType::Sizeof;
		keywordMap["typeof"]    = TokenType::Typeof;
		keywordMap["typeid"]    = TokenType::Typeid;
		keywordMap["static"]    = TokenType::Static;
		keywordMap["mut"]       = TokenType::Mutable;
		keywordMap["free"]      = TokenType::Dealloc;
		keywordMap["private"]   = TokenType::Private;
		keywordMap["virtual"]   = TokenType::Virtual;
		keywordMap["internal"]  = TokenType::Internal;
		keywordMap["override"]  = TokenType::Override;
		keywordMap["operator"]  = TokenType::Operator;
		keywordMap["continue"]  = TokenType::Continue;
		keywordMap["typealias"] = TokenType::TypeAlias;
		keywordMap["extension"] = TokenType::Extension;
		keywordMap["namespace"] = TokenType::Namespace;
		keywordMap["ffi"]       = TokenType::ForeignFunc;
	}






	TokenType getNextToken(const util::FastInsertVector<string_view>& lines, size_t* line, size_t* offset, const string_view& whole,
		Location& pos, Token* out, bool crlf)
	{
		bool flag = true;

		if(*line == lines.size())
		{
			out->loc = pos;
			out->type = TokenType::EndOfFile;
			return TokenType::EndOfFile;
		}

		string_view stream = lines[*line].substr(*offset);
		if(stream.empty())
		{
			out->loc = pos;
			out->type = TokenType::EndOfFile;
			return TokenType::EndOfFile;
		}



		size_t read = 0;
		size_t unicodeLength = 0;

		// first eat all whitespace
		skipWhitespace(stream, pos, offset);

		Token& tok = *out;
		tok.loc = pos;
		tok.type = TokenType::Invalid;


		// check compound symbols first.
		if(hasPrefix(stream, "//"))
		{
			tok.type = TokenType::Comment;
			// stream = stream.substr(0, 0);
			(*line)++;
			pos.line++;
			pos.col = 0;


			(*offset) = 0;

			// don't assign lines[line] = stream, since over here we've changed 'line' to be the next one.
			flag = false;
			tok.text = "";
		}
		else if(hasPrefix(stream, "=="))
		{
			tok.type = TokenType::EqualsTo;
			tok.text = "==";
			read = 2;
		}
		else if(hasPrefix(stream, ">="))
		{
			tok.type = TokenType::GreaterEquals;
			tok.text = ">=";
			read = 2;
		}
		else if(hasPrefix(stream, "<="))
		{
			tok.type = TokenType::LessThanEquals;
			tok.text = "<=";
			read = 2;
		}
		else if(hasPrefix(stream, "!="))
		{
			tok.type = TokenType::NotEquals;
			tok.text = "!=";
			read = 2;
		}
		else if(hasPrefix(stream, "||"))
		{
			tok.type = TokenType::LogicalOr;
			tok.text = "||";
			read = 2;
		}
		else if(hasPrefix(stream, "&&"))
		{
			tok.type = TokenType::LogicalAnd;
			tok.text = "&&";
			read = 2;
		}
		else if(hasPrefix(stream, "<-"))
		{
			tok.type = TokenType::LeftArrow;
			tok.text = "<-";
			read = 2;
		}
		else if(hasPrefix(stream, "->"))
		{
			tok.type = TokenType::RightArrow;
			tok.text = "->";
			read = 2;
		}
		else if(hasPrefix(stream, "<="))
		{
			tok.type = TokenType::FatLeftArrow;
			tok.text = "<=";
			read = 2;
		}
		else if(hasPrefix(stream, "=>"))
		{
			tok.type = TokenType::FatRightArrow;
			tok.text = "=>";
			read = 2;
		}
		else if(hasPrefix(stream, "++"))
		{
			tok.type = TokenType::DoublePlus;
			tok.text = "++";
			read = 2;
		}
		else if(hasPrefix(stream, "--"))
		{
			tok.type = TokenType::DoubleMinus;
			tok.text = "--";
			read = 2;
		}
		else if(hasPrefix(stream, "+="))
		{
			tok.type = TokenType::PlusEq;
			tok.text = "+=";
			read = 2;
		}
		else if(hasPrefix(stream, "-="))
		{
			tok.type = TokenType::MinusEq;
			tok.text = "-=";
			read = 2;
		}
		else if(hasPrefix(stream, "*="))
		{
			tok.type = TokenType::MultiplyEq;
			tok.text = "*=";
			read = 2;
		}
		else if(hasPrefix(stream, "/="))
		{
			tok.type = TokenType::DivideEq;
			tok.text = "/=";
			read = 2;
		}
		else if(hasPrefix(stream, "%="))
		{
			tok.type = TokenType::ModEq;
			tok.text = "%=";
			read = 2;
		}
		else if(hasPrefix(stream, "&="))
		{
			tok.type = TokenType::AmpersandEq;
			tok.text = "&=";
			read = 2;
		}
		else if(hasPrefix(stream, "|="))
		{
			tok.type = TokenType::PipeEq;
			tok.text = "|=";
			read = 2;
		}
		else if(hasPrefix(stream, "^="))
		{
			tok.type = TokenType::CaretEq;
			tok.text = "^=";
			read = 2;
		}
		else if(hasPrefix(stream, "::"))
		{
			tok.type = TokenType::DoubleColon;
			tok.text = "::";
			read = 2;
		}
		else if(hasPrefix(stream, "..."))
		{
			tok.type = TokenType::Ellipsis;
			tok.text = "...";
			read = 3;
		}
		else if(hasPrefix(stream, "..<"))
		{
			tok.type = TokenType::HalfOpenEllipsis;
			tok.text = "..<";
			read = 3;
		}
		else if(hasPrefix(stream, "/*"))
		{
			int currentNest = 1;

			// support nested, so basically we have to loop until we find either a /* or a */
			stream.remove_prefix(2);
			(*offset) += 2;
			pos.col += 2;


			Location opening = pos;
			Location curpos = pos;

			size_t k = 0;
			while(currentNest > 0)
			{
				// we can do this, because we know the closing token (*/) is 2 chars long
				// so if we have 1 char left, gg.
				if(k + 1 == stream.size() || stream[k] == '\n')
				{
					if(*line + 1 == lines.size())
						error(opening, "expected closing */ (reached EOF), for block comment started here:");

					// else, get the next line.
					// also note: if we're in this loop, we're inside a block comment.
					// since the ending token cannot be split across lines, we know that this last char
					// must also be part of the comment. hence, just skip over it.

					k = 0;
					curpos.line++;
					curpos.col = 0;
					(*offset) = 0;
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
				error(opening, "expected closing */ (reached EOF), for block comment started here:");

			pos = curpos;

			// don't actually store the text, because it's pointless and memory-wasting
			// tok.text = "/* I used to be a comment like you, until I took a memory-leak to the knee. */";
			tok.type = TokenType::Comment;
			tok.text = "";
			read = k;
		}
		else if(hasPrefix(stream, "*/"))
		{
			unexpected(tok.loc, "'*/'");
		}



		// attrs
		else if(hasPrefix(stream, "@nomangle"))
		{
			tok.type = TokenType::Attr_NoMangle;
			tok.text = "@nomangle";
			read = 9;
		}
		else if(hasPrefix(stream, "@entry"))
		{
			tok.type = TokenType::Attr_EntryFn;
			tok.text = "@entry";
			read = 6;
		}
		else if(hasPrefix(stream, "@raw"))
		{
			tok.type = TokenType::Attr_Raw;
			tok.text = "@raw";
			read = 4;
		}
		else if(hasPrefix(stream, "@operator"))
		{
			tok.type = TokenType::Attr_Operator;
			tok.text = "@operator";
			read = 9;
		}
		else if(hasPrefix(stream, "@platform"))
		{
			tok.type = TokenType::Attr_Platform;
			tok.text = "@platform";
			read = 9;
		}

		// directives
		else if(hasPrefix(stream, "#if"))
		{
			tok.type = TokenType::Directive_If;
			tok.text = "#if";
			read = 3;
		}
		else if(hasPrefix(stream, "#run"))
		{
			tok.type = TokenType::Directive_Run;
			tok.text = "#run";
			read = 4;
		}


		// unicode stuff
		else if(hasPrefix(stream, "ƒ"))
		{
			tok.type = TokenType::Func;
			read = std::string("ƒ").length();
			tok.text = "ƒ";

			unicodeLength = 1;
		}
		else if(hasPrefix(stream, "ﬁ"))
		{
			tok.type = TokenType::ForeignFunc;
			read = std::string("ﬁ").length();
			tok.text = "ﬁ";

			unicodeLength = 1;
		}
		else if(hasPrefix(stream, "÷"))
		{
			tok.type = TokenType::Divide;
			read = std::string("÷").length();
			tok.text = "÷";

			unicodeLength = 1;
		}
		else if(hasPrefix(stream, "≠"))
		{
			tok.type = TokenType::NotEquals;
			read = std::string("≠").length();
			tok.text = "≠";

			unicodeLength = 1;
		}
		else if(hasPrefix(stream, "≤"))
		{
			tok.type = TokenType::LessThanEquals;
			read = std::string("≤").length();
			tok.text = "≤";

			unicodeLength = 1;
		}
		else if(hasPrefix(stream, "≥"))
		{
			tok.type = TokenType::GreaterEquals;
			read = std::string("≥").length();
			tok.text = "≥";

			unicodeLength = 1;
		}

		else if(hasPrefix(stream, "'") && stream.size() > 2)
		{
			tok.type = TokenType::CharacterLiteral;

			if(stream[1] == '\\')
			{
				switch(stream[2])
				{
					case 'n':   tok.text = "\n"; break;
					case 'b':   tok.text = "\b"; break;
					case 'a':   tok.text = "\a"; break;
					case 'r':   tok.text = "\r"; break;
					case 't':   tok.text = "\t"; break;
					case '\'':  tok.text = "'"; break;
					case '\\':  tok.text = "\\"; break;

					default:
						error(pos, "invalid escape sequence ('\\%c') in character literal", stream[2]);
				}

				read = 4;
			}
			else
			{
				tok.text = stream.substr(1, 1);
				read = 3;
			}

			if(stream[read - 1] != '\'')
				error(pos, "expected closing '");
		}

		// note some special-casing is needed to differentiate between unary +/- and binary +/-
		// cases where we want binary:
		// ...) + 3   |   ...] + 3   |   ident + 3   |   number + 3   |   string + 3
		// so in every other case we want unary +/-.
		// note: a sane implementation would just return false if isdigit() was passed something weird, like a negative number
		// (because we tried to dissect a UTF-8 codepoint). so we just check if it's ascii first, which would solve the issue.
		else if((!stream.empty() && (isascii(stream[0]) && isdigit(stream[0]) || shouldConsiderUnaryLiteral(stream, pos)))
			/* handle cases like '+ 3' or '- 14' (ie. space between sign and number) */
			&& ((isascii(stream[0]) && isdigit(stream[0]) ? true : false) || (stream.size() > 1 && isascii(stream[1]) && isdigit(stream[1]))))
		{
			// copy it.
			auto tmp = stream;

			if(tmp.find('-') == 0 || tmp.find('+') == 0)
				tmp.remove_prefix(1);

			int base = 10;
			if(tmp.find("0x") == 0 || tmp.find("0X") == 0)
				base = 16, tmp.remove_prefix(2);

			else if(tmp.find("0b") == 0 || tmp.find("0B") == 0)
				base = 2, tmp.remove_prefix(2);


			// find that shit
			auto end = std::find_if_not(tmp.begin(), tmp.end(), [base](const char& c) -> bool {
				if(base == 10)	return isdigit(c);
				if(base == 16)	return isdigit(c) || (toupper(c) >= 'A' && toupper(c) <= 'F');
				else			return (c == '0' || c == '1');
			});

			tmp.remove_prefix((end - tmp.begin()));

			// check if we have 'e' or 'E'
			bool hadExp = false;
			if(tmp.size() > 0 && (tmp[0] == 'e' || tmp[0] == 'E'))
			{
				if(base != 10)
					error("exponential form is supported with neither hexadecimal nor binary literals");

				// find that shit
				auto next = std::find_if_not(tmp.begin() + 1, tmp.end(), isdigit);

				// this does the 'e' as well.
				tmp.remove_prefix(next - tmp.begin());

				hadExp = true;
			}

			size_t didRead = stream.size() - tmp.size();
			auto post = stream.substr(didRead);

			if(!post.empty() && post[0] == '.')
			{
				if(base != 10)
					error("invalid floating point literal; only valid in base 10");

				else if(hadExp)
					error("invalid floating point literal; decimal point cannot occur after the exponent ('e' or 'E').");

				// if the previous token was a '.' as well, then we're doing some tuple access
				// eg. x.0.1 (we would be at '0', having a period both ahead and behind us)

				// if the next token is not a number, then same thing, eg.
				// x.0.z, where the first tuple element of 'x' is a struct or something.

				// so -- lex a floating point *iff* the previous token was not '.', and the next token is a digit.
				if(prevType != TokenType::Period && post.size() > 1 && isdigit(post[1]))
				{
					// yes, parse a floating point
					post.remove_prefix(1), didRead++;

					while(post.size() > 0 && isdigit(post.front()))
						post.remove_prefix(1), didRead++;

					// ok.
				}
				else
				{
					// no, just return the integer token.
					// (which we do below, so just do nothing here)
				}
			}

			tok.text = stream.substr(0, didRead);

			tok.type = TokenType::Number;
			tok.loc.len = didRead;

			read = didRead;
		}
		else if(!stream.empty() && (stream[0] == '_'  || utf8iscategory(stream.data(), stream.size(), UTF8_CATEGORY_LETTER) > 0))
		{
			// get as many letters as possible first
			size_t identLength = utf8iscategory(stream.data(), stream.size(),
				UTF8_CATEGORY_LETTER | UTF8_CATEGORY_PUNCTUATION_CONNECTOR | UTF8_CATEGORY_NUMBER);

			read = identLength;
			tok.text = stream.substr(0, identLength);

			initKeywordMap();
			if(auto it = keywordMap.find(tok.text); it != keywordMap.end())
				tok.type = it->second;

			else
				tok.type = TokenType::Identifier;
		}
		else if(!stream.empty() && stream[0] == '"')
		{
			// string literal
			// because we want to avoid using std::string (ie. copying) in the lexer (Token), we must send the string over verbatim.

			// store the starting position
			size_t start = (size_t) (stream.data() - whole.data() + 1);

			// opening "
			pos.col++;

			size_t i = 1;
			size_t didRead = 0;
			for(; stream[i] != '"'; i++)
			{
				if(stream[i] == '\\')
				{
					if(i + 1 == stream.size())
					{
						unexpected(pos, "end of input");
					}
					else if(stream[i + 1] == '"')
					{
						// add the quote and the backslash, and skip it.
						didRead += 2;
						pos.col += 2;
						i++;
					}
					// breaking string over two lines
					else if(stream[i + 1] == '\n')
					{
						// skip it, then move to the next line
						pos.line++;
						pos.col = 0;
						(*line)++;

						if(*line == lines.size())
							unexpected(pos, "end of input");

						i = 0;

						// just a fudge factor gotten from empirical evidence
						// 3 extra holds for multiple lines, so all is well.

						didRead += 3;
						stream = lines[*line];
						(*offset) = 0;
					}
					else if(stream[i + 1] == '\\')
					{
						i++;
						didRead += 2;
						pos.col += 2;
					}
					else
					{
						// just put the backslash in.
						// and don't skip the next one.
						didRead++;
						pos.col++;
					}

					continue;
				}

				didRead++;
				pos.col++;

				if(i == stream.size() - 1 || stream[i] == '\n')
				{
					error(pos, "expected closing '\"' (%zu/%zu/%zu/%c/%s/%zu)", i, stream.size(), didRead,
						stream[i], util::to_string(stream), *offset);
				}
			}

			// closing "
			pos.col++;


			tok.type = TokenType::StringLiteral;
			tok.text = whole.substr(start, didRead);

			tok.loc.len = 2 + didRead;

			stream = stream.substr(i + 1);
			(*offset) += i + 1;

			read = 0;
			flag = false;
		}
		else if(crlf && hasPrefix(stream, "\r\n"))
		{
			read = 2;
			flag = false;

			tok.type = TokenType::NewLine;
			tok.text = "\n";
		}
		else if(!stream.empty())
		{
			if(isascii(stream[0]))
			{
				// check the first char
				switch(stream[0])
				{
					// for single-char things
					case '\n':  tok.type = TokenType::NewLine;      break;
					case '{':   tok.type = TokenType::LBrace;       break;
					case '}':   tok.type = TokenType::RBrace;       break;
					case '(':   tok.type = TokenType::LParen;       break;
					case ')':   tok.type = TokenType::RParen;       break;
					case '[':   tok.type = TokenType::LSquare;      break;
					case ']':   tok.type = TokenType::RSquare;      break;
					case '<':   tok.type = TokenType::LAngle;       break;
					case '>':   tok.type = TokenType::RAngle;       break;
					case '+':   tok.type = TokenType::Plus;         break;
					case '-':   tok.type = TokenType::Minus;        break;
					case '*':   tok.type = TokenType::Asterisk;     break;
					case '/':   tok.type = TokenType::Divide;       break;
					case '\'':  tok.type = TokenType::SQuote;       break;
					case '.':   tok.type = TokenType::Period;       break;
					case ',':   tok.type = TokenType::Comma;        break;
					case ':':   tok.type = TokenType::Colon;        break;
					case '=':   tok.type = TokenType::Equal;        break;
					case '?':   tok.type = TokenType::Question;     break;
					case '!':   tok.type = TokenType::Exclamation;  break;
					case ';':   tok.type = TokenType::Semicolon;    break;
					case '&':   tok.type = TokenType::Ampersand;    break;
					case '%':   tok.type = TokenType::Percent;      break;
					case '|':   tok.type = TokenType::Pipe;         break;
					case '@':   tok.type = TokenType::At;           break;
					case '#':   tok.type = TokenType::Pound;        break;
					case '~':   tok.type = TokenType::Tilde;        break;
					case '^':   tok.type = TokenType::Caret;        break;
					case '$':   tok.type = TokenType::Dollar;       break;

					default:
						error(tok.loc, "unknown token '%c'", stream[0]);
				}

				tok.text = stream.substr(0, 1);
				// tok.loc.col += 1;
				read = 1;
			}
			else if(utf8iscategory(stream.data(), stream.size(), UTF8_CATEGORY_SYMBOL_MATH | UTF8_CATEGORY_PUNCTUATION_OTHER) > 0)
			{
				read = utf8iscategory(stream.data(), stream.size(), UTF8_CATEGORY_SYMBOL_MATH | UTF8_CATEGORY_PUNCTUATION_OTHER);

				tok.text = stream.substr(0, read);
				tok.type = TokenType::UnicodeSymbol;
			}
			else
			{
				error(tok.loc, "unknown token '%s'", util::to_string(stream.substr(0, 10)));
			}
		}

		stream.remove_prefix(read);

		if(flag)
			(*offset) += read;

		if(tok.type != TokenType::NewLine)
		{
			if(read > 0)
			{
				// note(debatable): put the actual "position" in the front of the token
				pos.col += (unicodeLength > 0 ? unicodeLength : read);

				// special handling -- things like ƒ, ≤ etc. are one character wide, but can be several *bytes* long.
				pos.len = (unicodeLength > 0 ? unicodeLength : read);
				tok.loc.len = (unicodeLength > 0 ? unicodeLength : read);
			}
		}
		else
		{
			pos.col = 0;
			pos.line++;

			(*line)++;
			(*offset) = 0;
		}

		// debuglog("token %s: %d // %d\n", util::to_string(tok.text), tok.loc.col, pos.col);

		prevType = tok.type;
		prevID = tok.loc.fileID;

		return prevType;
	}
}







