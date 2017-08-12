// parser_internal.h
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once

#include "defs.h"
#include "errors.h"

#include "ast.h"
#include "lexer.h"

namespace pts
{
	struct Type;
}

namespace parser
{
	struct State
	{
		State(const lexer::TokenList& tl) : tokens(tl) { }

		const lexer::Token& lookahead(size_t num) const
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

		const lexer::Token& pop()
		{
			if(this->index + 1 < this->tokens.size())
				return this->tokens[this->index++];

			else
				error("pop() on last token");
		}

		const lexer::Token& eat()
		{
			this->skipWS();
			if(this->index + 1 < this->tokens.size())
				return this->tokens[this->index++];

			else
				error("eat() on last token");
		}

		void skipWS()
		{
			while(this->tokens[this->index] == lexer::TokenType::NewLine || this->tokens[this->index] == lexer::TokenType::Comment)
				this->index++;
		}

		const lexer::Token& front() const
		{
			return this->tokens[this->index];
		}

		const lexer::Token& frontAfterWS()
		{
			this->skipWS();
			return this->front();
		}

		const lexer::Token& prev()
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
			return this->front() == lexer::TokenType::Comment || this->front() == lexer::TokenType::NewLine;
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
			const lexer::TokenList& tokens;
	};

	pts::Type* parseType(State& st);
	ast::Expr* parseExpr(State& st);
	ast::Stmt* parseStmt(State& st);

	ast::Stmt* parseVariable(State& st);
	ast::ImportStmt* parseImport(State& st);
	ast::FuncDefn* parseFunction(State& st);

	ast::Block* parseBracedBlock(State& st);

	ast::LitNumber* parseNumber(State& st);

	ast::TopLevelBlock* parseTopLevel(State& st, std::string name);


	std::map<std::string, TypeConstraints_t> parseGenericTypeList(State& st);



	// error shortcuts

	// Expected $, found '$' instead
	void expected(const Location& loc, std::string, std::string) __attribute__((noreturn));

	// Expected $ after $, found '$' instead
	void expectedAfter(const Location& loc, std::string, std::string, std::string) __attribute__((noreturn));
}

























