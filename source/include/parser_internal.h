// parser_internal.h
// Copyright (c) 2014 - 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#pragma once

#include "defs.h"
#include "errors.h"

#include "ast.h"
#include "lexer.h"
#include "parser.h"

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
				error("lookahead %d tokens > size %d", num, this->tokens.size());
		}

		void skip(size_t num)
		{
			if(this->index + num < this->tokens.size())
				this->index += num;

			else
				error("skip %d tokens > size %d", num, this->tokens.size());
		}

		void rewind(size_t num)
		{
			if(this->index > num)
				this->index -= num;

			else
				error("rewind %d tokens > index %d", num, this->index);
		}

		void rewindTo(size_t ix)
		{
			if(ix >= this->tokens.size())
				error("ix %d > size %d", ix, this->tokens.size());

			this->index = ix;
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
			while(this->index < this->tokens.size() && (this->tokens[this->index] == lexer::TokenType::NewLine
				|| this->tokens[this->index] == lexer::TokenType::Comment))
			{
				this->index++;
			}
		}

		const lexer::Token& front() const
		{
			return this->tokens[this->index];
		}


		const lexer::Token& frontAfterWS()
		{
			size_t ofs = 0;
			while(this->tokens[this->index + ofs] == lexer::TokenType::NewLine
				|| this->tokens[this->index + ofs] == lexer::TokenType::Comment)
			{
				ofs++;
			}
			return this->tokens[this->index + ofs];
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

		size_t getIndex() const
		{
			return this->index;
		}

		void setIndex(size_t i)
		{
			iceAssert(i < tokens.size());
			this->index = i;
		}

		bool frontIsWS() const
		{
			return this->front() == lexer::TokenType::Comment || this->front() == lexer::TokenType::NewLine;
		}

		bool hasTokens() const
		{
			return this->index < this->tokens.size() && this->tokens[this->index] != lexer::TokenType::EndOfFile;
		}

		const lexer::TokenList& getTokenList()
		{
			return this->tokens;
		}


		void enterFunctionBody()
		{
			this->bodyNesting.push_back(1);
		}

		void leaveFunctionBody()
		{
			iceAssert(this->bodyNesting.size() > 0 && this->bodyNesting.back() == 1);
			this->bodyNesting.pop_back();
		}

		bool isInFunctionBody()
		{
			return this->bodyNesting.size() > 0 && this->bodyNesting.back() == 1;
		}



		void enterStructBody()
		{
			this->bodyNesting.push_back(2);
		}

		void leaveStructBody()
		{
			iceAssert(this->bodyNesting.size() > 0 && this->bodyNesting.back() == 2);
			this->bodyNesting.pop_back();
		}

		bool isInStructBody()
		{
			return this->bodyNesting.size() > 0 && this->bodyNesting.back() == 2;
		}


		void enterSubscript()
		{
			this->bodyNesting.push_back(3);
		}

		void leaveSubscript()
		{
			iceAssert(this->bodyNesting.size() > 0 && this->bodyNesting.back() == 3);
			this->bodyNesting.pop_back();
		}

		bool isInSubscript()
		{
			return this->bodyNesting.size() > 0 && this->bodyNesting.back() == 3;
		}


		void enterUsingParse()
		{
			this->bodyNesting.push_back(4);
		}

		void leaveUsingParse()
		{
			iceAssert(this->bodyNesting.size() > 0 && this->bodyNesting.back() == 4);
			this->bodyNesting.pop_back();
		}

		bool isParsingUsing()
		{
			return this->bodyNesting.size() > 0 && this->bodyNesting.back() == 4;
		}



		// implicitly coerce to location, so we can
		// error(st, ...)
		operator const Location&() const
		{
			return this->loc();
		}

		std::vector<ast::TypeDefn*> anonymousTypeDefns;

		std::string currentFilePath;

		util::hash_map<std::string, parser::CustomOperatorDecl> binaryOps;
		util::hash_map<std::string, parser::CustomOperatorDecl> prefixOps;
		util::hash_map<std::string, parser::CustomOperatorDecl> postfixOps;

		// flags that determine whether or not 'import' and '@operator' things can still be done.
		bool importsStillValid = true;
		bool operatorsStillValid = true;
		bool nativeWordSizeStillValid = true;

		frontend::CollectorState* cState = 0;

		private:
			// 1 = inside function
			// 2 = inside struct
			// 3 = inside subscript
			// 4 = inside using (specifically, in `using X as Y`, we're parsing 'X')
			std::vector<int> bodyNesting;

			size_t index = 0;
			const lexer::TokenList& tokens;
	};


	// like TCResult i guess.
	template <typename T>
	struct PResult
	{
		PResult(T* val)
		{
			this->state = 0;
			this->value = val;
		}

		explicit PResult(ErrorMsg* err, bool needsmore = false)
		{
			this->error = err;
			this->state = (needsmore ? 2 : 1);
		}

	private:
		explicit PResult(ErrorMsg* err, int stt)
		{
			this->error = err;
			this->state = stt;
		}
	public:


		PResult(const PResult& r)
		{
			this->state = r.state;
			if(this->state == 0)    this->value = r.value;
			else                    this->error = r.error;
		}

		PResult(PResult&& r) noexcept
		{
			this->state = r.state;
			r.state = -1;

			if(this->state == 0)    this->value = std::move(r.value);
			else                    this->error = std::move(r.error);
		}

		PResult& operator = (const PResult& r)
		{
			PResult tmp(r);
			*this = std::move(tmp);
			return *this;
		}

		PResult& operator = (PResult&& r) noexcept
		{
			if(&r != this)
			{
				this->state = r.state;
				r.state = -1;

				if(this->state == 0)    this->value = std::move(r.value);
				else                    this->error = std::move(r.error);
			}

			return *this;
		}

		// implicit conversion operator.
		template <typename A, typename X = std::enable_if_t<std::is_base_of_v<typename A::value_t, T>>>
		operator A() const
		{
			if(this->state == 0)    return PResult<typename A::value_t>(this->value);
			else                    return PResult<typename A::value_t>(this->error, this->state);
		}




		template <typename F>
		PResult<T> mutate(const F& fn)
		{
			if(this->state == 0) fn(this->value);
			return *this;
		}

		template <typename F, typename U = typename std::remove_pointer_t<std::result_of_t<F(T*)>>>
		PResult<U> map(const F& fn) const
		{
			if(this->state > 0) return PResult<U>(this->error, this->state);
			else                return PResult<U>(fn(this->value));
		}

		template <typename F, typename U = typename std::result_of_t<F(T*)>::value_t>
		PResult<U> flatmap(const F& fn) const
		{
			if(this->state > 0) return PResult<U>(this->error, this->state);
			else                return fn(this->value);
		}

		ErrorMsg* err() const
		{
			if(this->state < 1) compiler_crash("not error");
			else                return this->error;
		}

		T* val() const
		{
			if(this->state != 0)    this->error->postAndQuit();
			else                    return this->value;
		}

		bool isError() const
		{
			return this->state > 0;
		}

		bool hasValue() const
		{
			return this->state == 0;
		}

		bool needsMoreTokens() const
		{
			return this->state == 2;
		}

		static PResult<T> insufficientTokensError()
		{
			return PResult<T>(BareError::make("unexpected end of input"), /* needmore: */ true);
		}

		template <typename U>
		static PResult<T> copyError(const PResult<U>& other)
		{
			// only for error.
			if(other.state < 1)
				compiler_crash("not error");

			return PResult<T>(other.err(), other.state);
		}

		template <typename U>
		friend struct PResult;

		using value_t = T;

	private:
		// 0 = result, 1 = error, 2 = needsmoretokens
		int state;

		union {
			T* value;
			ErrorMsg* error;
		};
	};

	// Expected $, found '$' instead
	[[noreturn]] inline void expected(const Location& loc, std::string a, std::string b)
	{
		error(loc, "expected %s, found '%s' instead", a, b);
	}

	// Expected $ after $, found '$' instead
	[[noreturn]] inline void expectedAfter(const Location& loc, std::string a, std::string b, std::string c)
	{
		error(loc, "expected %s after %s, found '%s' instead", a, b, c);
	}

	// Unexpected $
	[[noreturn]] inline void unexpected(const Location& loc, std::string a)
	{
		error(loc, "unexpected %s", a);
	}



	std::string parseStringEscapes(const Location& loc, const std::string& str);

	std::string parseOperatorTokens(State& st);

	pts::Type* parseType(State& st);
	ast::Expr* parseExpr(State& st);
	PResult<ast::Stmt> parseStmt(State& st, bool allowExprs = true);


	ast::DeferredStmt* parseDefer(State& st);

	ast::Stmt* parseVariable(State& st);
	ast::ReturnStmt* parseReturn(State& st);
	ast::ImportStmt* parseImport(State& st);
	PResult<ast::FuncDefn> parseFunction(State& st);
	PResult<ast::Stmt> parseStmtWithAccessSpec(State& st);
	ast::ForeignFuncDefn* parseForeignFunction(State& st);
	ast::OperatorOverloadDefn* parseOperatorOverload(State& st);

	std::vector<std::pair<std::string, ast::Expr*>> parseCallArgumentList(State& st);

	ast::UsingStmt* parseUsingStmt(State& st);

	DecompMapping parseArrayDecomp(State& st);
	DecompMapping parseTupleDecomp(State& st);

	std::tuple<ast::FuncDefn*, bool, Location> parseFunctionDecl(State& st);
	ast::PlatformDefn* parsePlatformDefn(State& st);

	ast::RunDirective* parseRunDirective(State& st);

	ast::TraitDefn* parseTrait(State& st);
	ast::EnumDefn* parseEnum(State& st);
	ast::ClassDefn* parseClass(State& st);
	ast::StaticDecl* parseStaticDecl(State& st);

	PResult<ast::StructDefn> parseStruct(State& st, bool nameless);
	ast::UnionDefn* parseUnion(State& st, bool israw, bool nameless);

	ast::Expr* parseDollarExpr(State& st);

	ast::InitFunctionDefn* parseInitFunction(State& st);
	ast::InitFunctionDefn* parseDeinitFunction(State& st);
	ast::InitFunctionDefn* parseCopyOrMoveInitFunction(State& st, const std::string& name);

	ast::DeallocOp* parseDealloc(State& st);
	ast::SizeofOp* parseSizeof(State& st);

	PResult<ast::Block> parseBracedBlock(State& st);

	ast::LitNumber* parseNumber(State& st);
	ast::LitString* parseString(State& st, bool israw);
	ast::LitArray* parseArray(State& st, bool israw);

	ast::Stmt* parseForLoop(State& st);
	PResult<ast::Stmt> parseIfStmt(State& st);
	PResult<ast::WhileLoop> parseWhileLoop(State& st);

	ast::TopLevelBlock* parseTopLevel(State& st, const std::string& name);

	ast::Stmt* parseBreak(State& st);
	ast::Stmt* parseContinue(State& st);

	ast::Expr* parseCaretOrColonScopeExpr(State& st);

	std::vector<std::pair<std::string, TypeConstraints_t>> parseGenericTypeList(State& st);

	PolyArgMapping_t parsePAMs(State& st, bool* failed);
	AttribSet parseAttributes(State& st);

	std::tuple<std::vector<ast::FuncDefn::Param>, std::vector<std::pair<std::string, TypeConstraints_t>>,
		pts::Type*, bool, Location> parseFunctionLookingDecl(State& st);

	std::vector<std::string> parseIdentPath(const lexer::TokenList& tokens, size_t* idx);
}

























