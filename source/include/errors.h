// errors.h
// Copyright (c) 2014 - 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#pragma once

#include "defs.h"

#include "precompile.h"

template <typename... Ts>
inline void debuglog(const char* s, Ts&&... ts)
{
	auto out = tinyformat::format(s, ts...);
	fprintf(stderr, "%s", out.c_str());
}

template <typename... Ts>
inline void debuglogln(const char* s, Ts&&... ts)
{
	auto out = tinyformat::format(s, ts...);
	fprintf(stderr, "%s\n", out.c_str());
}


// error shortcuts

// Expected $, found '$' instead
[[noreturn]] void expected(const Location& loc, std::string, std::string);

// Expected $ after $, found '$' instead
[[noreturn]] void expectedAfter(const Location& loc, std::string, std::string, std::string);

// Unexpected $
[[noreturn]] void unexpected(const Location& loc, std::string);


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
#define FLOAT128_TYPE_STRING			"f128"

#define STRING_TYPE_STRING				"string"
#define CHARACTER_TYPE_STRING			"char"
#define CHARACTER_SLICE_TYPE_STRING		"str"

#define UNICODE_STRING_TYPE_STRING		"ustring"
#define UNICODE_CHARACTER_TYPE_STRING	"rune"

#define BOOL_TYPE_STRING				"bool"
#define VOID_TYPE_STRING				"void"

#define ANY_TYPE_STRING					"any"








namespace ast { struct Expr; }
namespace sst { struct Expr; }

namespace frontend
{
	const std::string& getFilenameFromID(size_t fileID);
	std::string getFilenameFromPath(const std::string& path);
}


std::string __error_gen_internal(const Location& loc, const std::string& msg, const char* type, bool context);

template <typename... Ts>
std::string __error_gen(const Location& loc, const char* msg, const char* type, bool, Ts&&... ts)
{
	return __error_gen_internal(loc, tinyformat::format(msg, ts...), type, true);
}


#define ERROR_FUNCTION(name, type, attr, doexit)                                                                        \
template <typename... Ts> [[attr]] void name (const char* fmt, Ts&&... ts)                                              \
{ fputs(__error_gen(Location(), fmt, type, doexit, ts...).c_str(), stderr); if(doexit) doTheExit(false); }              \
																														\
template <typename... Ts> [[attr]] void name (Locatable* e, const char* fmt, Ts&&... ts)                                \
{ fputs(__error_gen(e ? e->loc : Location(), fmt, type, doexit, ts...).c_str(), stderr); if(doexit) doTheExit(false); } \
																														\
template <typename... Ts> [[attr]] void name (const Location& loc, const char* fmt, Ts&&... ts)                         \
{ fputs(__error_gen(loc, fmt, type, doexit, ts...).c_str(), stderr); if(doexit) doTheExit(false); }



ERROR_FUNCTION(error, "error", noreturn, true);
ERROR_FUNCTION(info, "note", maybe_unused, false);
ERROR_FUNCTION(warn, "warning", maybe_unused, false);
















