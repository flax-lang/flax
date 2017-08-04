// parser.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
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

		const Token& lookahead(size_t num)
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

		const Token& eat()
		{
			if(this->index + 1 < this->tokens.size())
				return this->tokens[this->index++];

			else error("next() on last token");
		}

		void skipWS()
		{
			while(this->tokens[this->index] == TokenType::NewLine || this->tokens[this->index] == TokenType::Comment)
				this->index++;
		}

		const Token& front()
		{
			return this->tokens[this->index];
		}

		const Token& frontAfterWS()
		{
			this->skipWS();
			return this->front();
		}

		const Location& loc()
		{
			return this->front().loc;
		}

		const Location& ploc()
		{
			iceAssert(this->index > 0);
			return this->tokens[this->index - 1].loc;
		}

		private:
			size_t index = 0;
			const TokenList& tokens;
	};

	// definitions
	static ImportStmt* parseImport(State& st)
	{

	}







	static TopLevelBlock* parseTopLevel(State& st, std::string name = "")
	{
		using TTy = TokenType;
		TopLevelBlock* root = new TopLevelBlock(st.loc(), name);

		// if it's not empty, then it's an actual user-defined namespace
		if(name != "")
		{
			// expect "namespace FOO { ... }"
			iceAssert(st.eat() == TTy::Identifier);
			if(st.eat() != TTy::LBrace)
				error(st.ploc(), "Expected '{' to start namespace declaration");
		}

		switch(st.front())
		{
			case TTy::Import: {

				// root->statements.push_back(parseImport(st));

			} break;

			case TTy::Namespace: {

				st.eat();
				Token tok;
				if((tok = st.frontAfterWS()) != TTy::Identifier)
					error(st.loc(), "Expected identifier after 'namespace'");

				root->statements.push_back(parseTopLevel(st, tok.str()));

			} break;

			case TTy::Func: {

			} break;


			default: {
				error("Unexpected token '%s'", st.front().str().c_str());
			}
		}

		if(name != "")
		{
			if(st.eat() != TTy::RBrace)
				error(st.loc(), "Expected '}' to close namespace declaration");
		}

		debuglog("parsed namespace '%s'", name.c_str());
		return root;
	}














	ParsedFile parseFile(std::string filename)
	{
		const TokenList& tokens = frontend::getFileTokens(filename);
		auto state = State(tokens);

		auto toplevel = parseTopLevel(state);
	}
}
















