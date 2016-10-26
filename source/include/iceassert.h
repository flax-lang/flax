// iceassert.h
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>

inline void error_and_exit(const char* s, ...) __attribute__((noreturn));
inline void error_and_exit(const char* s, ...)
{
	va_list ap;
	va_start(ap, s);

	char* alloc = nullptr;
	vasprintf(&alloc, s, ap);

	fprintf(stderr, "%s%s%s%s: %s%s\n", "\033[1m\033[31m", "Error", "\033[0m", "\033[1m", alloc, "\033[0m");

	// vfprintf(stderr, s, ap);

	free(alloc);

	va_end(ap);
	abort();
}

inline void debuglog(const char* s, ...) __attribute__((format(printf, 1, 2)));
inline void debuglog(const char* s, ...)
{
	va_list ap;
	va_start(ap, s);
	vfprintf(stderr, s, ap);
	va_end(ap);
}



#define __nothing
#define iceAssert(x)		((x) ? ((void) (0)) : error_and_exit("Compiler assertion at %s:%d, cause:\n'%s' evaluated to false\n", __FILE__, __LINE__, #x))


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

#define FUNC_KEYWORD_STRING				"func"






