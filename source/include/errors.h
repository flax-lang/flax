// errors.h
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

namespace Ast
{
	struct Expr;
}


void __error_gen(uint64_t line, uint64_t col, uint64_t len, const char* file, const char* msg, const char* type,
	bool doExit, va_list ap);

void error(const char* msg, ...) __attribute__((noreturn, format(printf, 1, 2)));
void error(Ast::Expr* e, const char* msg, ...) __attribute__((noreturn, format(printf, 2, 3)));

void errorNoExit(const char* msg, ...) __attribute__((format(printf, 1, 2)));
void errorNoExit(Ast::Expr* e, const char* msg, ...) __attribute__((format(printf, 2, 3)));

void warn(const char* msg, ...) __attribute__((format(printf, 1, 2)));
void warn(Ast::Expr* e, const char* msg, ...) __attribute__((format(printf, 2, 3)));

void info(const char* msg, ...) __attribute__((format(printf, 1, 2)));
void info(Ast::Expr* e, const char* msg, ...) __attribute__((format(printf, 2, 3)));


#define __nothing
#define iceAssert(x)		((x) ? (void) (0) : error("Compiler assertion at %s:%d, cause:\n'%s' evaluated to false", __FILE__, __LINE__, #x))
