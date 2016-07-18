// errors.h
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

#include "defs.h"

namespace Ast
{
	struct Expr;
}

namespace Parser
{
	struct Token;
	struct ParserState;
}

struct HighlightOptions
{
	Parser::Pin caret;
	std::deque<Parser::Pin> underlines;
	bool drawCaret;

	HighlightOptions() : drawCaret(true) { }
	HighlightOptions(Parser::Pin c) : caret(c), underlines({ }), drawCaret(true) { }
	HighlightOptions(Parser::Pin c, std::deque<Parser::Pin> u) : caret(c), underlines(u), drawCaret(true) { }
	HighlightOptions(Parser::Pin c, std::deque<Parser::Pin> u, bool dc) : caret(c), underlines(u), drawCaret(dc) { }
};

Parser::Pin getHighlightExtent(Ast::Expr* e);

void doTheExit() __attribute__ ((noreturn));

void __error_gen(HighlightOptions ops, const char* msg, const char* type,
	bool doExit, va_list ap);

void error(const char* msg, ...) __attribute__((noreturn, format(printf, 1, 2)));
void error(Ast::Expr* e, const char* msg, ...) __attribute__((noreturn, format(printf, 2, 3)));
void error(Ast::Expr* e, HighlightOptions ops, const char* msg, ...) __attribute__((noreturn, format(printf, 3, 4)));

void errorNoExit(const char* msg, ...) __attribute__((format(printf, 1, 2)));
void errorNoExit(Ast::Expr* e, const char* msg, ...) __attribute__((format(printf, 2, 3)));
void errorNoExit(Ast::Expr* e, HighlightOptions ops, const char* msg, ...) __attribute__((format(printf, 3, 4)));

void warn(const char* msg, ...) __attribute__((format(printf, 1, 2)));
void warn(Ast::Expr* e, const char* msg, ...) __attribute__((format(printf, 2, 3)));
void warn(Ast::Expr* e, HighlightOptions ops, const char* msg, ...) __attribute__((format(printf, 3, 4)));

void info(const char* msg, ...) __attribute__((format(printf, 1, 2)));
void info(Ast::Expr* e, const char* msg, ...) __attribute__((format(printf, 2, 3)));
void info(Ast::Expr* e, HighlightOptions ops, const char* msg, ...) __attribute__((format(printf, 3, 4)));



enum class Err
{
	Info,
	Warn,
	Error,
};

void parserMessage(Err severity, const char* msg, ...)							__attribute__((format(printf, 2, 3)));
void parserMessage(Err severity, Parser::Pin pin, const char* msg, ...)			__attribute__((format(printf, 3, 4)));
void parserMessage(Err severity, Parser::Token tok, const char* msg, ...)		__attribute__((format(printf, 3, 4)));
void parserMessage(Err severity, Parser::ParserState& ps, const char* msg, ...)	__attribute__((format(printf, 3, 4)));

void parserError(const char* msg, ...)											__attribute__((format(printf, 1, 2), noreturn));
void parserError(Parser::Pin pin, const char* msg, ...)							__attribute__((format(printf, 2, 3), noreturn));
void parserError(Parser::Token tok, const char* msg, ...)						__attribute__((format(printf, 2, 3), noreturn));
void parserError(Parser::ParserState& ps, const char* msg, ...)					__attribute__((format(printf, 2, 3), noreturn));















