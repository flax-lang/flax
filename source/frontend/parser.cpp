// parser.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "pts.h"
#include "errors.h"
#include "parser.h"
#include "frontend.h"

using namespace lexer;
using namespace ast;
namespace parser
{
	struct State
	{
		State(const TokenList& tl) : tokens(tl) { }

		const Token& lookahead(size_t num) const
		{
			if(this->index + num < this->tokens.size())
				return this->tokens[this->index + num];

			else
				error("lookahead %zu tokens > size %zu", num, this->tokens.size());
		}

		const void skip(size_t num)
		{
			if(this->index + num < this->tokens.size())
				this->index += num;

			else
				error("skip %zu tokens > size %zu", num, this->tokens.size());
		}

		const Token& pop()
		{
			if(this->index + 1 < this->tokens.size())
				return this->tokens[this->index++];

			else
				error("pop() on last token");
		}

		const Token& eat()
		{
			this->skipWS();
			if(this->index + 1 < this->tokens.size())
				return this->tokens[this->index++];

			else
				error("eat() on last token");
		}

		void skipWS()
		{
			while(this->tokens[this->index] == TokenType::NewLine || this->tokens[this->index] == TokenType::Comment)
				this->index++;
		}

		const Token& front() const
		{
			return this->tokens[this->index];
		}

		const Token& frontAfterWS()
		{
			this->skipWS();
			return this->front();
		}

		const Token& prev()
		{
			return this->tokens[this->index - 1];
		}

		const Location& loc() const
		{
			return this->front().loc;
		}

		const Location& ploc() const
		{
			iceAssert(this->index > 0);
			return this->tokens[this->index - 1].loc;
		}

		size_t remaining() const
		{
			return this->tokens.size() - this->index - 1;
		}

		bool frontIsWS() const
		{
			return this->front() == TokenType::Comment || this->front() == TokenType::NewLine;
		}

		bool hasTokens() const
		{
			return this->index < this->tokens.size();
		}




		// implicitly coerce to location, so we can
		// error(st, ...)
		operator const Location&() const
		{
			return this->loc();
		}

		private:
			size_t index = 0;
			const TokenList& tokens;
	};

	// definitions
	static ImportStmt* parseImport(State& st)
	{
		using TT = TokenType;
		iceAssert(st.eat() == TT::Import);

		if(st.frontAfterWS() == TT::StringLiteral)
		{
			return new ImportStmt(st.loc(), st.frontAfterWS().str());
		}
		else if(st.frontAfterWS() == TT::Identifier)
		{
			std::string name;
			auto loc = st.loc();
			while(st.front() == TokenType::Identifier)
			{
				name += st.eat().str();

				if(st.front() == TokenType::Period)
				{
					name += "/";
					st.eat();
				}
				else if(st.frontIsWS() || st.front() == TT::Semicolon)
				{
					break;
				}
				else
				{
					error(st, "Unexpected token '%s' in module specifier for import statement",
						st.front().str().c_str());
				}
			}

			// i hope this works.
			return new ImportStmt(loc, name);
		}
		else
		{
			error(st, "Expected either string literal or identifer after 'import' for module specifier, found '%s' instead",
				st.frontAfterWS().str().c_str());
		}
	}



	static pts::Type* parseType(State& st);

	static TopLevelBlock* parseTopLevel(State& st, std::string name)
	{
		using TT = TokenType;
		TopLevelBlock* root = new TopLevelBlock(st.loc(), name);

		// if it's not empty, then it's an actual user-defined namespace
		if(name != "")
		{
			// expect "namespace FOO { ... }"
			iceAssert(st.eat() == TT::Identifier);
			if(st.eat() != TT::LBrace)
				error(st.ploc(), "Expected '{' to start namespace declaration, found '%s'", st.lookahead(-1).str().c_str());
		}

		while(st.hasTokens() && st.front() != TT::EndOfFile)
		{
			switch(st.front())
			{
				case TT::Import: {

					root->statements.push_back(parseImport(st));

				} break;

				case TT::Namespace: {
					st.eat();
					Token tok;
					if((tok = st.front()) != TT::Identifier)
						error(st, "Expected identifier after 'namespace'");

					root->statements.push_back(parseTopLevel(st, tok.str()));

				} break;

				case TT::Func: {

				} break;

				case TT::Var:
				case TT::Val: {
					st.eat();

					debuglog(">> %s\n", parseType(st)->str().c_str());
				} break;

				case TT::Comment:
				case TT::NewLine:
					break;

				case TT::RBrace:
					goto out;

				default: {
					error(st, "Unexpected token '%s' / %d", st.front().str().c_str(), st.front().type);
				}
			}

			st.skipWS();
		}

		out:
		if(name != "")
		{
			if(st.front() != TT::RBrace)
				error(st, "Expected '}' to close namespace declaration, found '%d' instead", st.front().type);

			st.eat();
		}

		debuglog("parsed namespace '%s'\n", name.c_str());
		return root;
	}














	ParsedFile parseFile(std::string filename)
	{
		const TokenList& tokens = frontend::getFileTokens(filename);
		auto state = State(tokens);

		auto toplevel = parseTopLevel(state, "");

		auto parsedFile = ParsedFile();
		parsedFile.name = filename;
		parsedFile.root = toplevel;

		return parsedFile;
	}








	static pts::Type* parseTypeIndirections(State& st, pts::Type* base)
	{
		using TT = TokenType;
		auto ret = base;

		while(st.front() == TT::Asterisk)
			ret = new pts::PointerType(ret), st.pop();

		if(st.front() == TT::LSquare)
		{
			// parse an array of some kind
			st.pop();

			if(st.front() == TT::RSquare)
			{
				ret = new pts::DynamicArrayType(ret);
			}
			else if(st.front() == TT::Ellipsis)
			{
				st.pop();
				if(st.eat() != TT::RSquare)
					error(st, "Expected closing ']' after variadic array type, found '%s' instead", st.prev().str().c_str());

				ret = new pts::VariadicArrayType(ret);
			}
			else if(st.front() == TT::Number)
			{
				long sz = std::stol(st.front().str());
				if(sz <= 0)
					error(st, "Expected positive, non-zero size for fixed array, found '%s' instead", st.front().str().c_str());

				st.pop();
				if(st.eat() != TT::RSquare)
					error(st, "Expected closing ']' after array type, found '%s' instead", st.front().str().c_str());

				ret = new pts::FixedArrayType(ret, sz);
			}
			else if(st.front() == TT::Colon)
			{
				st.pop();
				if(st.eat() != TT::RSquare)
					error(st, "Expected closing ']' after slice type, found '%s' instead", st.prev().str().c_str());

				ret = new pts::ArraySliceType(ret);
			}
			else
			{
				error(st, "Unexpected token '%s' after opening '['; expected some kind of array type",
					st.front().str().c_str());
			}
		}

		if(st.front() == TT::LSquare || st.front() == TT::Asterisk)
			return parseTypeIndirections(st, ret);

		else
			return ret;
	}

	static pts::Type* parseType(State& st)
	{
		using TT = TokenType;
		if(st.front() == TT::Identifier)
		{
			std::string s = st.eat().str();

			while(st.hasTokens())
			{
				if(st.front() == TT::Period)
				{
					s += ".", st.eat();
				}
				else if(st.front() == TT::Identifier)
				{
					if(s.back() != '.')
						error(st, "Unexpected identifer '%s' in type", st.front().str().c_str());

					else
						s += st.eat().str();
				}
				else
				{
					break;
				}
			}

			auto nt = pts::NamedType::create(s);

			// check generic mapping
			if(st.front() == TT::LAngle)
			{
				// ok
				st.pop();
				while(st.hasTokens())
				{
					if(st.front() == TT::Identifier)
					{
						std::string ty = st.eat().str();
						if(st.eat() != TT::Colon)
						{
							error(st, "Expected ':' to specify type mapping in parametric type instantiation; found '%s' instead",
								st.prev().str().c_str());
						}

						pts::Type* mapped = parseType(st);
						nt->genericMapping[ty] = mapped;


						if(st.front() == TT::Comma)
						{
							st.pop();
							continue;
						}
						else if(st.front() == TT::RAngle)
						{
							break;
						}
						else
						{
							error(st, "Expected either ',' or '>' to continue or terminate parametric type instantiation, found '%s' instead",
								st.front().str().c_str());
						}
					}
					else if(st.front() == TT::RAngle)
					{
						error(st, "Need at least one type mapping in parametric type instantiation");
					}
					else
					{
						// error(st, "Unexpected token '%s' in type mapping", st.front().str().c_str());
						break;
					}
				}

				if(st.front() != TT::RAngle)
					error(st, "Expected '>' to end type mapping, found '%s' instead", st.front().str().c_str());

				st.pop();
			}


			// check for indirections
			return parseTypeIndirections(st, nt);
		}
		else if(st.front() == TT::LParen)
		{
			// tuple or function
			st.pop();

			// parse a tuple.
			std::vector<pts::Type*> types;
			while(st.hasTokens() && st.front().type != TT::RParen)
			{
				// do things.
				auto ty = parseType(st);

				if(st.front() != TT::Comma && st.front() != TT::RParen)
					error("Unexpected token '%s' in type specifier, expected either ',' or ')'", st.front().str().c_str());

				else if(st.front() == TT::Comma)
					st.eat();

				types.push_back(ty);
			}

			if(types.size() == 0)
				error("Empty tuples '()' are not supported");

			if(st.eat().type != TT::RParen)
			{
				error("Expected ')' to end tuple type specifier, found '%s' instead",
					st.prev().str().c_str());
			}

			// check if it's actually a function type
			if(st.front() != TT::Arrow)
			{
				// this *should* allow us to 'group' types together
				// eg. ((i64, i64) -> i64)[] would allow us to create an array of functions
				// whereas (i64, i64) -> i64[] would parse as a function returning an array of i64s.

				if(types.size() == 1)
					return parseTypeIndirections(st, types[0]);

				auto tup = new pts::TupleType(types);
				return parseTypeIndirections(st, tup);
			}
			else
			{
				// eat the arrow, parse the type
				st.eat();
				auto rty = parseType(st);

				auto ft = new pts::FunctionType(types, rty);
				return parseTypeIndirections(st, ft);
			}
		}
		else
		{
			error(st, "Unexpected token '%s' while parsing type", st.front().str().c_str());
		}
	}
}
















