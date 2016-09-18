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
	vfprintf(stderr, s, ap);
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


#define INTUNSPEC_TYPE_STRING	"int"
#define INT8_TYPE_STRING		"int8"
#define INT16_TYPE_STRING		"int16"
#define INT32_TYPE_STRING		"int32"
#define INT64_TYPE_STRING		"int64"

#define UINTUNSPEC_TYPE_STRING	"uint"
#define UINT8_TYPE_STRING		"uint8"
#define UINT16_TYPE_STRING		"uint16"
#define UINT32_TYPE_STRING		"uint32"
#define UINT64_TYPE_STRING		"uint64"

#define FLOAT32_TYPE_STRING		"float32"
#define FLOAT64_TYPE_STRING		"float64"

#define FLOAT_TYPE_STRING		"float"

#define BOOL_TYPE_STRING		"bool"
#define VOID_TYPE_STRING		"void"

#define FUNC_KEYWORD_STRING		"func"

