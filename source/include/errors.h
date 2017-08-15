// errors.h
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>

#include <vector>

#include "defs.h"

inline void debuglog(const char* s, ...) __attribute__((format(printf, 1, 2)));
inline void debuglog(const char* s, ...)
{
	va_list ap;
	va_start(ap, s);
	vfprintf(stderr, s, ap);
	va_end(ap);
}

// error shortcuts

// Expected $, found '$' instead
void expected(const Location& loc, std::string, std::string) __attribute__((noreturn));

// Expected $ after $, found '$' instead
void expectedAfter(const Location& loc, std::string, std::string, std::string) __attribute__((noreturn));

// Unexpected $
void unexpected(const Location& loc, std::string) __attribute__((noreturn));


#define INTUNSPEC_TYPE_STRING			"int"
#define UINTUNSPEC_TYPE_STRING			"uint"

#define FLOAT_TYPE_STRING				"float"
#define DOUBLE_TYPE_STRING				"double"

#define INT8_TYPE_STRING				"i8"
#define INT16_TYPE_STRING				"i16"
#define INT32_TYPE_STRING				"i32"
#define INT64_TYPE_STRING				"i64"
#define INT128_TYPE_STRING				"i128"

#define UINT8_TYPE_STRING				"u8"
#define UINT16_TYPE_STRING				"u16"
#define UINT32_TYPE_STRING				"u32"
#define UINT64_TYPE_STRING				"u64"
#define UINT128_TYPE_STRING				"u128"

#define FLOAT32_TYPE_STRING				"f32"
#define FLOAT64_TYPE_STRING				"f64"
#define FLOAT80_TYPE_STRING				"f80"
#define FLOAT128_TYPE_STRING			"f128"

#define STRING_TYPE_STRING				"string"
#define CHARACTER_TYPE_STRING			"char"

#define UNICODE_STRING_TYPE_STRING		"ustring"
#define UNICODE_CHARACTER_TYPE_STRING	"rune"

#define BOOL_TYPE_STRING				"bool"
#define VOID_TYPE_STRING				"void"

#define ANY_TYPE_STRING					"any"








namespace ast
{
	struct Expr;
}


struct HighlightOptions
{
	Location caret;
	std::vector<Location> underlines;
	bool drawCaret;

	HighlightOptions() : drawCaret(true) { }
	HighlightOptions(const Location& c) : caret(c), underlines({ }), drawCaret(true) { }
	HighlightOptions(const Location& c, std::vector<Location> u) : caret(c), underlines(u), drawCaret(true) { }
	HighlightOptions(const Location& c, std::vector<Location> u, bool dc) : caret(c), underlines(u), drawCaret(dc) { }
};

// Location getHighlightExtent(ast::Expr* e);

void doTheExit() __attribute__ ((noreturn));

void __error_gen(HighlightOptions ops, const char* msg, const char* type,
	bool doExit, va_list ap);

void error(const char* msg, ...) __attribute__((noreturn, format(printf, 1, 2)));
void error(ast::Expr* e, const char* msg, ...) __attribute__((noreturn, format(printf, 2, 3)));
void error(ast::Expr* e, HighlightOptions ops, const char* msg, ...) __attribute__((noreturn, format(printf, 3, 4)));
void error(const Location& loc, const char* msg, ...) __attribute__((noreturn, format(printf, 2, 3)));

void exitless_error(const char* msg, ...) __attribute__((format(printf, 1, 2)));
void exitless_error(ast::Expr* e, const char* msg, ...) __attribute__((format(printf, 2, 3)));
void exitless_error(ast::Expr* e, HighlightOptions ops, const char* msg, ...) __attribute__((format(printf, 3, 4)));
void exitless_error(const Location& loc, const char* msg, ...) __attribute__((format(printf, 2, 3)));

void warn(const char* msg, ...) __attribute__((format(printf, 1, 2)));
void warn(ast::Expr* e, const char* msg, ...) __attribute__((format(printf, 2, 3)));
void warn(ast::Expr* e, HighlightOptions ops, const char* msg, ...) __attribute__((format(printf, 3, 4)));
void warn(const Location& loc, const char* msg, ...) __attribute__((format(printf, 2, 3)));

void info(const char* msg, ...) __attribute__((format(printf, 1, 2)));
void info(ast::Expr* e, const char* msg, ...) __attribute__((format(printf, 2, 3)));
void info(ast::Expr* e, HighlightOptions ops, const char* msg, ...) __attribute__((format(printf, 3, 4)));
void info(const Location& loc, const char* msg, ...) __attribute__((format(printf, 2, 3)));



